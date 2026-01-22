#include "PhysScene.h"
#include "PhysXHelper.h"
#include "PhysicsFilter.h"
#include "Transform.h"
#include "CollisionMatrix.h"

bool MMMEngine::PhysScene::Create(const PhysSceneDesc& desc)
{
	m_desc = desc;

	//Dispatcher(thread 설정)
	uint32_t threads = (m_desc.cpuThreadCount == 0) ? 2u : m_desc.cpuThreadCount;
	m_dispatcher = physx::PxDefaultCpuDispatcherCreate(threads);

	if (!m_dispatcher) return false;

	if (!CreateScene())
	{
		SAFE_RELEASE(m_dispatcher);
		return false;
	}

	//if (m_desc.enablePVD) {
	//	//SetupPvdFlags();
	//}

	return true;
}

bool MMMEngine::PhysScene::CreateScene()
{
	auto& physics = PhysicX::Get().GetPhysics();

	physx::PxSceneDesc pxDesc(physics.getTolerancesScale());
	pxDesc.gravity = ToPxVec(m_desc.gravity);
	pxDesc.cpuDispatcher = m_dispatcher;
	pxDesc.filterShader = CustomFilterShader;
	pxDesc.simulationEventCallback = &m_callback;

	m_scene = PhysicX::Get().GetPhysics().createScene(pxDesc);
	if (m_scene == nullptr) return false;

	return true;
}

void MMMEngine::PhysScene::Destroy()
{
	if (m_scene)
	{
		std::vector<RigidBodyComponent*> rigidsCopy;
		rigidsCopy.reserve(m_rigids.size());
		for (auto* rb : m_rigids) rigidsCopy.push_back(rb);

		for (auto* rb : rigidsCopy)
			UnregisterRigid(rb);

		// 남아 있을 수 있는 컨테이너 강제 정리
		m_ownerByCollider.clear();
		m_collidersByRigid.clear();
		m_rigids.clear();

		m_scene->release();
		m_scene = nullptr;
	}

	if (m_dispatcher)
	{
		m_dispatcher->release();
		m_dispatcher = nullptr;
	}

	m_frameContacts.clear();
	m_frameTriggers.clear();
}

void MMMEngine::PhysScene::Step(float dt)
{
	if (!m_scene) return;
	if (dt <= 0.0f) return;

	m_scene->simulate(dt);
	m_scene->fetchResults(true);

	// PhysX -> Engine Transform 동기화를 여기서 한다면 이부분에 넣으면됨
	// PullFromPhysics는 무조건 fetchResults 이후에만 하면 된다
	/*for (auto* rb : m_rigids)
	{
		if (rb) rb->PullFromPhysics();
	}*/

	//콜백이벤트 drain ( 비우기 )
	m_frameContacts.clear(); m_frameTriggers.clear();

	m_callback.DrainContacts(m_frameContacts);
	m_callback.DrainTriggers(m_frameTriggers);
}

void MMMEngine::PhysScene::AddActor(physx::PxActor& actor)
{
	if (!m_scene) return;
	m_scene->addActor(actor);
}

void MMMEngine::PhysScene::RemoveActor(physx::PxActor& actor)
{
	if (!m_scene) return;
	m_scene->removeActor(actor);
}

void MMMEngine::PhysScene::RegisterRigid(MMMEngine::RigidBodyComponent* rb)
{
	if (!m_scene) return;
	if (!rb) return;

	// 이미 등록되어 있으면 중복 방지
	if (m_rigids.find(rb) != m_rigids.end())
		return;


	// actor가 없으면 생성 (actor가 없다 -> 내부적rigid를 등록할때임 -> transform은 매니저에서 자동으로 설정하도록 함 )
	// 사용자가 rigid없이 collider부터 생성을했을때 매니저에서 이를 판단하고 내부적 rigid를 매니저에서 관리 및 액터생성/등록은 scene를 통해서함
	if (rb->GetPxActor() == nullptr)
	{
		Vector3 t_pos = rb->GetTransform()->GetWorldPosition();
		Quaternion t_quater = rb->GetTransform()->GetWorldRotation();
		 rb->CreateActor(&PhysicX::Get().GetPhysics(), t_pos, t_quater);
	}

	auto* actor = rb->GetPxActor();
	if (!actor) return;

	// PxScene에 추가
	m_scene->addActor(*actor);

	// 컨테이너 등록
	m_rigids.insert(rb);
}

void MMMEngine::PhysScene::UnregisterRigid(MMMEngine::RigidBodyComponent* rb)
{
	if (!m_scene) return;
	if (!rb) return;

	auto itR = m_rigids.find(rb);
	if (itR == m_rigids.end())
		return; // 등록 안 됨

	// rb에 붙은 콜라이더 전부 detach (컨테이너 정리 포함)
	auto itList = m_collidersByRigid.find(rb);
	if (itList != m_collidersByRigid.end())
	{
		// detach 중에 벡터가 바뀔 수 있으니 복사
		auto cols = itList->second;
		for (auto* col : cols)
		{
			if (!col) continue;
			DetachCollider(rb, col);
		}
	}

	// actor scene에서 제거
	if (auto* actor = rb->GetPxActor())
	{
		m_scene->removeActor(*actor);
	}

	// 컨테이너 정리
	m_collidersByRigid.erase(rb);
	m_rigids.erase(itR);
	//actor release는 rb가 하도록
	rb->DestroyActor();
}

//void MMMEngine::PhysScene::SwapRigid(MMMEngine::RigidBodyComponent* oldRb, MMMEngine::RigidBodyComponent* newRb, const CollisionMatrix& matrix)
//{
//	//oldRb에 붙은 collider 목록을 복사
//	auto it = m_collidersByRigid.find(oldRb);
//	std::vector<ColliderComponent*> cols;
//	if (it != m_collidersByRigid.end())
//		cols = it->second;
//
//	//oldRb에서 전부 detach
//	for (auto* col : cols)
//		DetachCollider(oldRb, col);
//
//	//oldRb unregister (actor 제거)
//	UnregisterRigid(oldRb);
//
//	//newRb register
//	RegisterRigid(newRb);
//
//	//collider를 newRb에 attach
//	for (auto* col : cols)
//		AttachCollider(newRb, col, matrix);
//}

void MMMEngine::PhysScene::AttachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col, const CollisionMatrix& matrix)
{
	if (!m_scene) return;
	if (!rb || !col) return;

	// rb가 등록 안 돼있으면 먼저 등록
	if (m_rigids.find(rb) == m_rigids.end())
	{
		//AttachCollider called for unregistered rigid. RegisterRigid must be called first.
		assert(false && "actor가 미등록 상태입니다 actor를 먼저 등록하고 collider를 붙여야합니다");
		return;
	}

	//col이 이미 다른 rb에 붙어있으면 먼저 detach
	auto itOwner = m_ownerByCollider.find(col);
	if (itOwner != m_ownerByCollider.end())
	{
		if (itOwner->second == rb)
		{
			// 이미 같은 rb에 붙어있음 → 중복 attach 방지
			return;
		}
		else
		{
			//해당 collider가 다른 rb에 붙어있음
			std::cout << "해당 collider는 다른 rb에 붙어있던 collider임 확인필요" << std::endl;
			DetachCollider(itOwner->second, col);
		}
	}

	// shape 없으면 생성(또는 rebuild)
	if (col->GetPxShape() == nullptr)
	{
		assert(false && "Shape가 없는 상태 로직 확인이 필요");
	}



	// 자식 콜라이더 로컬포즈 반영이 필요하면 여기서 setLocalPose 해줘야 함
	// 예: shape->setLocalPose( ToPxTrans(colLocalOffsetPos, colLocalOffsetRot) );
	// (col이 rb 기준 로컬오프셋을 계산할 수 있는 함수가 필요)
	// Todo: 로컬포즈 반영이 필요하다면 여기서 코드 작성


	// 필터 적용
	auto* shape = col->GetPxShape();
	if (!shape) return;

	uint32_t layer = 0;
	if constexpr (true)
	{
		layer = col->GetEffectiveLayer();
	}

	col->SetFilterData(matrix.MakeSimFilter(layer), matrix.MakeQueryFilter(layer));

	// rb(actor)에 attach
	rb->AttachCollider(col);
	m_scene->resetFiltering(*rb->GetPxActor());

	// 컨테이너 갱신
	m_ownerByCollider[col] = rb;

	auto& vec = m_collidersByRigid[rb];
	if (!Contains(vec, col))
		vec.push_back(col);
}


void MMMEngine::PhysScene::DetachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col)
{
	if (!rb || !col) return;

	// 소유자가 다르면 ownerByCollider 기준으로 정정
	auto itOwner = m_ownerByCollider.find(col);
	if (itOwner == m_ownerByCollider.end())
		return; // attach 상태 아님

	if (itOwner->second != rb)
	{
	#ifdef _DEBUG
		OutputDebugStringA("[PhysScene] DetachCollider: non-owner rb passed. Using actual owner.\n");
	#endif
		rb = itOwner->second;
	}

	// rb(actor)에서 detach
	rb->DetachCollider(col);

	// 컨테이너 정리
	m_ownerByCollider.erase(col);

	auto itList = m_collidersByRigid.find(rb);
	if (itList != m_collidersByRigid.end())
	{
		EraseOne(itList->second, col);
		if (itList->second.empty())
			m_collidersByRigid.erase(itList);
	}
}


void MMMEngine::PhysScene::ReapplyFilters(const CollisionMatrix& matrix)
{
	std::unordered_set<MMMEngine::RigidBodyComponent*> touchedRigids;
	touchedRigids.reserve(m_rigids.size());

	for (auto& kv : m_ownerByCollider)
	{
		MMMEngine::ColliderComponent* col = kv.first;
		MMMEngine::RigidBodyComponent* rb = kv.second;
		if (!col || !rb) continue;

		auto* shape = col->GetPxShape();
		if (!shape) continue;

		uint32_t layer = col->GetEffectiveLayer();
		col->SetFilterData(matrix.MakeSimFilter(layer), matrix.MakeQueryFilter(layer));

		touchedRigids.insert(rb);
	}

	// resetFiltering은 rb(=actor) 단위로 1번씩만
	for (auto* rb : touchedRigids)
	{
		if (!rb) continue;
		if (auto* actor = rb->GetPxActor())
		{
			m_scene->resetFiltering(*actor);
		}
	}
}

void MMMEngine::PhysScene::UpdateColliderGeometry(MMMEngine::ColliderComponent* col)
{
	if (!m_scene) return;
	if (!col) return;

	if (!col->IsGeometryDirty()) return;

	physx::PxShape* shape = col->GetPxShape();
	if (!shape) return;

	//const bool ok = col->UpdateShapeGeometry();
//
//	if (ok)
//	{
//		col->SetGeometryDirty(false);
//	}
//#ifdef _DEBUG
//	else
//	{
//		OutputDebugStringA("[PhysScene] UpdateColliderGeometry: UpdateShapeGeometry failed.\n");
//	}
//#endif
}

void MMMEngine::PhysScene::RebuildCollider(MMMEngine::ColliderComponent* col, const CollisionMatrix& matrix)
{
	if (!m_scene || !col) return;

	//owner rb 찾기 (attach 상태가 아니라면 ownerByCollider에 없을 수 있음)
	MMMEngine::RigidBodyComponent* rb = nullptr;
	if (auto it = m_ownerByCollider.find(col); it != m_ownerByCollider.end())
		rb = it->second;

	//새 shape 생성 (ColliderComponent 쪽에서 생성/보유 포인터 갱신)
	//    - 여기서 "기존 shape release를 누가 하느냐" 정책이 중요함.
	//    - 추천: ColliderComponent가 자신의 old shape를 release하고 새로 만든 포인터로 교체.
	//      (PhysScene는 detach/attach만 책임)
	auto& physics = PhysicX::Get().GetPhysics();
	physx::PxMaterial* mat = PhysicX::Get().GetDefaultMaterial(); // 너 엔진 방식에 맞게

	// attach 상태면 actor에서 먼저 떼고 교체 후 다시 붙이는 게 안전
	if (rb && rb->GetPxActor() && col->GetPxShape())
	{
		rb->DetachCollider(col); // 내부적으로 actor->detachShape(*shape)까지 수행한다고 가정
	}

	//shape 재생성
	//    - col->BuildShape(...)가 col 내부 m_shape를 새로 만들게
	col->BuildShape(&physics, mat);

	auto* newShape = col->GetPxShape();
	if (!newShape) return;

	//필터 재적용(레이어 기반)
	uint32_t layer = col->GetEffectiveLayer();
	col->SetFilterData(matrix.MakeSimFilter(layer), matrix.MakeQueryFilter(layer));

	//다시 attach
	if (rb && rb->GetPxActor())
	{
		rb->AttachCollider(col); // actor->attachShape(*newShape)
	}
}

