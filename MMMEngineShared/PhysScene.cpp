#include "PhysScene.h"
#include "PhysXHelper.h"
#include "PhysicsFilter.h"
#include "Transform.h"
#include "CollisionMatrix.h"
#include "GameObject.h"
#include "PhysX.h"

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
	pxDesc.flags |= physx::PxSceneFlag::eENABLE_CCD;
	pxDesc.solverType = physx::PxSolverType::eTGS;

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
}

void MMMEngine::PhysScene::PullRigidsFromPhysics()
{
	// 여기서는 "유효한 rb + 유효한 actor"만 Pull
	for (auto* rb : m_rigids)
	{
		if (!rb) continue;

		// ObjPtr라면 IsValid()가 안전장치 역할. (댕글링이면 이조차 위험할 수 있는데,
		// 최종적으로는 rb 제거가 Notify/Unregister를 통해 들어오게 만드는 게 맞다.)
		if (!rb->GetGameObject().IsValid()) continue;

		if (!rb->GetPxActor()) continue;

		rb->PullFromPhysics();
	}
}

void MMMEngine::PhysScene::ApplyInterpolation(float alpha)
{
	for (auto* rb : m_rigids)
	{
		if (!rb) continue;
		if (!rb->GetGameObject().IsValid()) continue;
		if (!rb->GetPxActor()) continue;

		rb->ApplyInterpolation(alpha);
	}
}

void MMMEngine::PhysScene::SyncRigidsFromTransforms()
{
	for (auto* rb : m_rigids)
	{
		if (!rb) continue;
		if (!rb->GetGameObject().IsValid()) continue;

		auto tr = rb->GetTransform();
		if (!tr) continue;

		const Vector3 pos = tr->GetWorldPosition();
		const Quaternion rot = tr->GetWorldRotation();
		rb->Editor_changeTrans(pos, rot);
	}
}

void MMMEngine::PhysScene::DrainEvents()
{
	m_frameContacts.clear();
	m_frameTriggers.clear();

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
	if (!m_scene || !rb) return;
	if (m_rigids.find(rb) != m_rigids.end()) return;

	auto go = rb->GetGameObject();
	if (!go.IsValid()) return;

	// rb 내부 Transform 포인터 바인딩을 확정
	//rb->BindTransform(&go->GetTransform()); // 네 엔진 API에 맞게

	// actor 없으면 생성 (Transform은 go 기준)
	if (rb->GetPxActor() == nullptr)
	{
		auto& tr = go->GetTransform();
		rb->CreateActor(&PhysicX::Get().GetPhysics(),
			tr->GetWorldPosition(),
			tr->GetWorldRotation());
	}

	auto* actor = rb->GetPxActor();
	if (!actor) return;

	m_scene->addActor(*actor);
	m_rigids.insert(rb);
}

void MMMEngine::PhysScene::UnregisterRigid(MMMEngine::RigidBodyComponent* rb)
{
	if (!m_scene) return;
	if (!rb) return;

	auto itR = m_rigids.find(rb);
	if (itR == m_rigids.end())
		return; // 등록 안 됨


	// GameObject가 유효하지 않으면 컨테이너만 정리하고 반환
	if (!rb->GetGameObject().IsValid())
	{
		if (!m_scene || !rb) return;

		auto itR = m_rigids.find(rb);
		if (itR == m_rigids.end())
			return;

		//rb에 달린 콜라이더 목록 확보 (GO invalid여도 이 컨테이너 기반으로 정리 가능)
		std::vector<ColliderComponent*> cols;
		if (auto itList = m_collidersByRigid.find(rb); itList != m_collidersByRigid.end())
			cols = itList->second;

		//GO가 invalid이면 PhysX 호출은 위험할 수 있으니 "컨테이너만" 정리하고 종료
		if (!rb->GetGameObject().IsValid())
		{
			for (auto* col : cols)
			{
				if (!col) continue;

				// ownerByCollider에서 rb가 owner인 경우만 제거 (안전장치)
				auto itOwner = m_ownerByCollider.find(col);
				if (itOwner != m_ownerByCollider.end() && itOwner->second == rb)
					m_ownerByCollider.erase(itOwner);
			}

			m_collidersByRigid.erase(rb);
			m_rigids.erase(itR);
			return;
		}

		// ---- 여기부터는 GO valid인 정상 케이스 ----

		// rb에 붙은 콜라이더 전부 detach (PhysX detach + 컨테이너 정리)
		for (auto* col : cols)
		{
			if (!col) continue;
			DetachCollider(rb, col);
		}

		// actor scene에서 제거
		if (auto* actor = rb->GetPxActor())
		{
			m_scene->removeActor(*actor);
		}

		m_collidersByRigid.erase(rb);
		m_rigids.erase(itR);

		rb->DestroyActor();
	}

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
		try
		{
			m_scene->removeActor(*actor);
		}
		catch (...)
		{
			// actor가 이미 해제되었거나 유효하지 않은 경우 무시
		}
	}

	// 컨테이너 정리
	m_collidersByRigid.erase(rb);
	m_rigids.erase(itR);
	//actor release는 rb가 하도록
	if (rb->GetGameObject().IsValid())
	{
		rb->DestroyActor();
	}
}

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
	if (!col) return;

	// 소유자가 다르면 ownerByCollider 기준으로 정정
	auto itOwner = m_ownerByCollider.find(col);
	if (itOwner == m_ownerByCollider.end())
		return; // attach 상태 아님

	MMMEngine::RigidBodyComponent* ownerRb = itOwner->second;

	//ownerByCollider는 무조건 제거 actor에서 삭제책임은 collider가 하고있음
	m_ownerByCollider.erase(itOwner);

	//ownerRb가 없으면 끝내도 되는 작업
	if (!ownerRb) return;

	// rb가 nullptr이거나 owner가 다르면 실제 owner 사용
	if (!rb || rb != ownerRb)
		rb = ownerRb;;

	rb->DetachCollider(col);

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

//collider크기 변경시 매니저로부터 명령받는 함수
void MMMEngine::PhysScene::UpdateColliderGeometry(MMMEngine::ColliderComponent* col)
{
	if (!m_scene) return;
	if (!col) return;
	if (!col->IsGeometryDirty()) return;

	physx::PxShape* shape = col->GetPxShape();
	if (!shape)
	{
#ifdef _DEBUG
		OutputDebugStringA("[PhysScene] UpdateColliderGeometry: shape is null. skipped.\n");
#endif
		return;
	}

	const bool ok = col->UpdateShapeGeometry();

	if (ok)
	{
		col->SetGeometryDirty(false);
		col->RefreshCommonProps();
	}

#ifdef _DEBUG
	OutputDebugStringA("[PhysScene] UpdateColliderGeometry: UpdateShapeGeometry failed.\n");
#endif
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

void MMMEngine::PhysScene::PushRigidsToPhysics()
{
	for (auto* rb : m_rigids)
	{
		if (!rb) continue;
		if (!rb->GetGameObject().IsValid()) continue;
		rb->PushToPhysics(); // 내부에서 PoseDirty/ForceQueue 처리

		if (!rb->IsMassDirty()) continue;

		auto* a = rb->GetPxActor();
		if (!a) continue; // actor 없으면 다음 프레임에 다시 시도할 수 있게 dirty 유지

		if (auto* d = a->is<physx::PxRigidDynamic>())
		{
			physx::PxRigidBodyExt::updateMassAndInertia(*d, rb->GetMass());
			rb->ClearMassDirty();
		}
		else
		{
			// static이면 의미 없으니 지워도 됨
			rb->ClearMassDirty();
		}
	}
}

void MMMEngine::PhysScene::ChangeRigidType(MMMEngine::RigidBodyComponent* rb, const CollisionMatrix& matrix)
{
	if (!m_scene || !rb) return;
	if (!rb->HasPendingTypeChange()) return;

	const bool registered = (m_rigids.find(rb) != m_rigids.end());

	//기존 콜라이더 목록 확보 (포인터 복사만)
	std::vector<ColliderComponent*> cols;
	if (auto it = m_collidersByRigid.find(rb); it != m_collidersByRigid.end())
		cols = it->second;

	//기존 actor scene에서 제거
	if (registered)
	{
		if (auto* oldActor = rb->GetPxActor())
			m_scene->removeActor(*oldActor);
	}

	//기존 actor 파괴
	rb->DestroyActor();


	//새 actor 생성
	auto& physics = PhysicX::Get().GetPhysics();
	rb->SetType_Internal();
	rb->CreateActor(&physics, rb->GetRequestedPos(), rb->GetRequestedRot());

	auto* newActor = rb->GetPxActor();
	if (!newActor) return;

	//콜라이더 다시 attach (컨테이너는 건드리지 말고, actor에 shape만 attach)
	for (auto* col : cols)
	{
		if (!col) continue;

		auto* shape = col->GetPxShape();
		if (!shape) continue; // 정책상 여기서 BuildShape를 할지, 그냥 스킵할지 결정

		// 필터 재적용
		uint32_t layer = col->GetEffectiveLayer();
		col->SetFilterData(matrix.MakeSimFilter(layer), matrix.MakeQueryFilter(layer));

		// actor에 shape만 붙이기 (rb->AttachCollider(관리형) 대신)
		rb->AttachShapeOnly(shape);
	}

	//씬에 다시 등록
	if (registered)
	{
		m_scene->addActor(*newActor);
		m_scene->resetFiltering(*newActor);
	}

	//질량/관성 재계산은 dynamic일 때만
	if (auto* dyn = newActor->is<physx::PxRigidDynamic>())
		physx::PxRigidBodyExt::updateMassAndInertia(*dyn, rb->GetMass());
	rb->OffPendingType();
}

void MMMEngine::PhysScene::SetGravity(float x, float y, float z)
{
	m_desc.gravity[0] = x;
	m_desc.gravity[1] = y;
	m_desc.gravity[2] = z;

	if (m_scene)
	{
		m_scene->setGravity(ToPxVec(m_desc.gravity));
	}
}

void MMMEngine::PhysScene::ResetFilteringFor(MMMEngine::ColliderComponent* col)
{
	if (!m_scene || !col) return;

	// shape가 없으면 아직 PhysX에 붙어있지 않거나 생성 전
	physx::PxShape* shape = col->GetPxShape();
	if (!shape) return;

	// 이 콜라이더가 붙어있는 rb 찾기
	auto it = m_ownerByCollider.find(col);
	if (it == m_ownerByCollider.end()) return;

	MMMEngine::RigidBodyComponent* rb = it->second;
	if (!rb) return;

	physx::PxRigidActor* actor = rb->GetPxActor();
	if (!actor) return;

	// PhysX에 "필터링 다시 계산" 요청
	m_scene->resetFiltering(*actor);
}

