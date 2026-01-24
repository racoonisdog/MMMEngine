#include "RigidBodyComponent.h"
#include "PhysXHelper.h"
#include "Transform.h"
#include "rttr/registration"
#include "ColliderComponent.h"
#include "GameObject.h"
#include "PhysxManager.h"
#include "PhysX.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<RigidBodyComponent>("RigidBodyComponent")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<RigidBodyComponent>>()))
		.property("Type", &RigidBodyComponent::GetType , &RigidBodyComponent::SetType)
		.property("Mass", &RigidBodyComponent::GetMass , &RigidBodyComponent::SetMass)
		.property("LinearDamping", &RigidBodyComponent::GetLineDamping , &RigidBodyComponent::SetLineDamping)
		.property("AngularDamping", &RigidBodyComponent::GetAngularDamping , &RigidBodyComponent::SetAngularDamping)
		.property("UseGravity", &RigidBodyComponent::GetUseGravity , &RigidBodyComponent::SetUseGravity)
		.property("IsKinematic", &RigidBodyComponent::GetKinematic , &RigidBodyComponent::SetKinematic)
		;

	registration::class_<ObjPtr<RigidBodyComponent>>("ObjPtr<RigidBodyComponent>")
		.constructor(
			[]() {
				return Object::NewObject<RigidBodyComponent>();
			});
	registration::enumeration<RigidBodyComponent::Type>("RigidType")
		(
			rttr::value("Dynamic", RigidBodyComponent::Type::Dynamic),
			rttr::value("Static", RigidBodyComponent::Type::Static)
			);
	registration::class_<RigidBodyComponent::Desc>("RigidDesc")
		.property("Type", &RigidBodyComponent::Desc::type)
		.property("Mass", &RigidBodyComponent::Desc::mass)
		.property("LinearDamping", &RigidBodyComponent::Desc::linearDamping)
		.property("AngularDamping", &RigidBodyComponent::Desc::angularDamping)
		.property("UseGravity", &RigidBodyComponent::Desc::useGravity)
		.property("IsKinematic", &RigidBodyComponent::Desc::isKinematic);


	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<RigidBodyComponent>>();
}

void MMMEngine::RigidBodyComponent::CreateActor(physx::PxPhysics* physics, Vector3 worldPos, Quaternion Quater)
{
	if (m_Actor) return;

	m_Physics = physics;

	physx::PxTransform px_Trans = ToPxTrans(worldPos, Quater);

	if (m_Desc.type == Type::Static)
	{
		m_Actor = physics->createRigidStatic(px_Trans);
	}
	else
	{
		auto* t_dynamic = physics->createRigidDynamic(px_Trans);
		m_Actor = t_dynamic;

		t_dynamic->setLinearDamping(m_Desc.linearDamping);
		t_dynamic->setAngularDamping(m_Desc.angularDamping);
		t_dynamic->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !m_Desc.useGravity);

		if (m_Desc.isKinematic) {
			t_dynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, true);
		}
	}

	for (auto* t_collider : m_Colliders)
	{
		if (auto* shape = t_collider->GetPxShape())
			m_Actor->attachShape(*shape);
	}

	if (auto* t_dynamic = m_Actor->is<physx::PxRigidDynamic>()) {
		physx::PxRigidBodyExt::updateMassAndInertia(*t_dynamic, m_Desc.mass);
	}

	m_Actor->userData = this;
}

//생성했을때 이미 존재하면 등록안하도록 함
void MMMEngine::RigidBodyComponent::Initialize()
{
	auto it = GetGameObject()->GetComponentsCount<RigidBodyComponent>();
	if (it >= 2)
	{
		std::cout << u8"이미 존재하는 RigidComponent" << std::endl;
		Destroy(SelfPtr(this));
		return;
	}
	m_Desc = { MMMEngine::RigidBodyComponent::Type::Dynamic, 1.0f, 0.05f, 0.0f, true, false };

	
	//auto TransTarget = GetGameObject()->GetTransform();
	//TransTarget->onMatrixUpdate.AddListener<RigidBodyComponent, &RigidBodyComponent::BindTeleport>(this);
	m_ColliderMaster = true;
	MMMEngine::PhysxManager::Get().NotifyRigidAdded(this);
}

void MMMEngine::RigidBodyComponent::UnInitialize()
{
	if (m_ColliderMaster)
	{
		if(GetGameObject().IsValid())
		{
			auto objPtr = GetGameObject()->GetComponents<ColliderComponent>();
			for (auto& it : objPtr)
			{
				Destroy(it);
			}
		}
	}
	MMMEngine::PhysxManager::Get().NotifyRigidRemoved(this);
}

void MMMEngine::RigidBodyComponent::OnDestroy()
{
	//for (auto* col : m_Colliders) DetachCollider(col);

	/*auto cols = m_Colliders;
	for (auto* col : cols) DetachCollider(col);
	DestroyActor();

	m_Physics = nullptr;*/

	if (!m_Actor) return;
	m_Actor->release();
	m_Actor = nullptr;
	
	m_Physics = nullptr;
}

void MMMEngine::RigidBodyComponent::AttachCollider(ColliderComponent* collider)
{
	if (!collider) return;

	if (std::find(m_Colliders.begin(), m_Colliders.end(), collider) == m_Colliders.end())
		m_Colliders.push_back(collider);

	if (!m_Actor) return;

	if (auto* shape = collider->GetPxShape())
		m_Actor->attachShape(*shape);

	if (auto* d = m_Actor->is<physx::PxRigidDynamic>())
		physx::PxRigidBodyExt::updateMassAndInertia(*d, m_Desc.mass);
}

void MMMEngine::RigidBodyComponent::DetachCollider(ColliderComponent* collider)
{
	if (!collider) return;

	// 붙어있는 목록에서 제거
	auto it = std::find(m_Colliders.begin(), m_Colliders.end(), collider);
	if (it != m_Colliders.end())
		m_Colliders.erase(it);

	// actor가 있으면 PhysX shape도 detach
	if (m_Actor)
	{
		if (auto* shape = collider->GetPxShape())
			m_Actor->detachShape(*shape);

		if (auto* d = m_Actor->is<physx::PxRigidDynamic>())
			physx::PxRigidBodyExt::updateMassAndInertia(*d, m_Desc.mass);
	}
}

void MMMEngine::RigidBodyComponent::DestroyActor()
{
	if (!m_Actor) return;
	m_Actor->release();
	m_Actor = nullptr;
}

void MMMEngine::RigidBodyComponent::PushToPhysics()
{
	if (!m_Actor) return;
	if (!GetGameObject().IsValid()) return;
	PushPoseIfDirty();
	PushStateChanges();
	PushForces();
	PushWakeUp();
}

void MMMEngine::RigidBodyComponent::PullFromPhysics()
{
	if (!m_Actor) return;
	//if (!m_Tr) return; // 엔진 Transform 포인터(혹은 참조)가 연결된 경우만 ( 오류방지용 )


	// Static은 보통 Pull할 필요 없음 , 에디터 이동/텔레포트는 Push에서 setGlobalPose로 처리)
	if (m_Desc.type == Type::Static) return;

	auto* t_dynamic = m_Actor->is<physx::PxRigidDynamic>();
	if (!t_dynamic) return;

	//키네마틱은 코드 포지션 기준이라 Pull로 덮어쓰지 않음
	//동기화 옵션을 두고 켤 수있도록 가능
	//지금은 키네마틱이면 덮어쓰지 않도록 
	if (m_Desc.isKinematic)
	{
		// const physx::PxTransform pxPose = t_dynamic->getGlobalPose(); 동기화 옵션용
		// ApplyPoseToEngine(pxPose);
		return;
	}

	// PhysX -> Engine Transform
	const physx::PxTransform pxPose = t_dynamic->getGlobalPose();

	GetTransform()->SetWorldPosition(ToVec(pxPose.p));
	GetTransform()->SetWorldRotation(ToQuat(pxPose.q));

}

void MMMEngine::RigidBodyComponent::PushPoseIfDirty()
{
	if (!m_Actor) return;

	//텔레포트/에디터 강제 이동 요청을 처리하는 분기
	if (m_PoseDirty)
	{
		m_Actor->setGlobalPose(ToPxTrans(m_RequestedWorldPose.position, m_RequestedWorldPose.rotation));

		if (auto* t_dynamic = m_Actor->is<physx::PxRigidDynamic>())
		{
			if (!m_Desc.isKinematic)
			{
				t_dynamic->setLinearVelocity(physx::PxVec3(0));
				t_dynamic->setAngularVelocity(physx::PxVec3(0));
			}
		}
		m_PoseDirty = false;
	}

	//키네마틱 이동 요청이 있을때 처리하는 분기
	if (m_Desc.isKinematic && m_HasKinematicTarget)
	{
		if (auto* t_dynamic = m_Actor->is<physx::PxRigidDynamic>())
		{
			t_dynamic->setKinematicTarget(ToPxTrans(m_KinematicTarget.position, m_KinematicTarget.rotation));
		}
		m_HasKinematicTarget = false;
	}
}

void MMMEngine::RigidBodyComponent::PushForces()
{
	auto* t_dynamic = m_Actor->is<physx::PxRigidDynamic>();
	if (!t_dynamic) return;
	if (m_Desc.isKinematic) {
		m_ForceQueue.clear(); m_TorqueQueue.clear();
		m_ForceQueue.reserve(8); m_TorqueQueue.reserve(8); return;
	}

	for (const auto& cmd : m_ForceQueue)
	{
		t_dynamic->addForce(ToPxVec(cmd.vec), ToPxForceMode(cmd.mode));
	}

	for (const auto& cmd : m_TorqueQueue)
	{
		t_dynamic->addTorque(ToPxVec(cmd.vec), ToPxForceMode(cmd.mode));
	}

	m_ForceQueue.clear();
	m_TorqueQueue.clear();
	//8크기로 바로 재할당
	m_ForceQueue.reserve(8);
	m_TorqueQueue.reserve(8);

	t_dynamic->wakeUp();
}



void MMMEngine::RigidBodyComponent::Teleport(const Vector3& worldPos, const Quaternion& Quater)
{
	m_RequestedWorldPose.position = worldPos;
	m_RequestedWorldPose.rotation = Quater;
	m_PoseDirty = true;
	m_WakeRequested = true;
}

void MMMEngine::RigidBodyComponent::SetKinematicTarget(const Vector3& worldPos, const Quaternion& Quater)
{
	// 키네마틱 호출용 방어코드 다이나믹일경우 아웃
	if (m_Desc.type != Type::Dynamic) return;

	// 키네마틱 설정이 되었고 키네마틱좌표를 저장
	m_KinematicTarget.position = worldPos;
	m_KinematicTarget.rotation = Quater;

	m_HasKinematicTarget = true;
	m_WakeRequested = true;

	// 만약 호출 시점에 키네마틱 전환까지 같이 보장하고 싶으면 (까먹엇을수도 있으니깐)
	// m_Desc.isKinematic = true; 
	// m_DescDirty = true;
}

void MMMEngine::RigidBodyComponent::MoveKinematicTarget()
{
	if (m_Desc.type != Type::Dynamic) return;
	if (!m_Desc.isKinematic) return;

	// mesh자체의 transform을 목표로 삼는다
	m_KinematicTarget.position = GetTransform()->GetWorldPosition();
	m_KinematicTarget.rotation = GetTransform()->GetWorldRotation();

	m_HasKinematicTarget = true;
	m_WakeRequested = true;
}

void MMMEngine::RigidBodyComponent::Editor_changeTrans(const Vector3& worldPos, const Quaternion& Quater)
{
	m_RequestedWorldPose.position = worldPos;
	m_RequestedWorldPose.rotation = Quater;

	m_PoseDirty = true;
	m_WakeRequested = true;
}

void MMMEngine::RigidBodyComponent::AddForce(Vector3 f, ForceMode mod)
{
	//다이나믹이 아니면 힘을 줄 필요가 없음
	if (m_Desc.type != Type::Dynamic) return;
	//키네마틱이 true라면 힘을 줄 필요가 없음
	if (m_Desc.isKinematic) return;

	//바로 처리하는게 아니라 vector에 넣음
	m_ForceQueue.push_back({ f, mod });

	m_WakeRequested = true;
}

void MMMEngine::RigidBodyComponent::AddTorque(Vector3 tor, ForceMode mod)
{
	if (m_Desc.type != Type::Dynamic) return;
	if (m_Desc.isKinematic) return;

	m_TorqueQueue.push_back({ tor, mod });
	m_WakeRequested = true;
}

void MMMEngine::RigidBodyComponent::PushStateChanges()
{
	if (!m_DescDirty || !m_Actor) return;

	m_Actor->setActorFlag(physx::PxActorFlag::eDISABLE_GRAVITY, !m_Desc.useGravity);

	if (auto* t_dynamic = m_Actor->is<physx::PxRigidDynamic>())
	{
		t_dynamic->setLinearDamping(m_Desc.linearDamping);
		t_dynamic->setAngularDamping(m_Desc.angularDamping);
		t_dynamic->setRigidBodyFlag(physx::PxRigidBodyFlag::eKINEMATIC, m_Desc.isKinematic);
		physx::PxRigidBodyExt::updateMassAndInertia(*t_dynamic, m_Desc.mass);
	}

	m_DescDirty = false;
}

void MMMEngine::RigidBodyComponent::PushWakeUp()
{
	if (!m_WakeRequested) return;
	m_WakeRequested = false;

	if (m_Desc.type != Type::Dynamic) return;
	if (auto* t_dynamic = m_Actor->is<physx::PxRigidDynamic>()) t_dynamic->wakeUp();
}

void MMMEngine::RigidBodyComponent::AddImpulse(Vector3 imp)
{
	AddForce(imp, ForceMode::Impulse);
	m_WakeRequested = true;
}

//선형 속도(Linear Velocity) 설정
void MMMEngine::RigidBodyComponent::SetLinearVelocity(Vector3 v)
{
	if (!m_Actor) return;
	if (m_Desc.type != Type::Dynamic) return;
	if (m_Desc.isKinematic) return;

	if (auto* d = m_Actor->is<physx::PxRigidDynamic>())
	{
		d->setLinearVelocity(ToPxVec(v));
		d->wakeUp();
	}
}

Vector3 MMMEngine::RigidBodyComponent::GetLinearVelocity() const
{
	if (!m_Actor) return Vector3();
	if (m_Desc.type != Type::Dynamic) return Vector3();

	if (auto* d = m_Actor->is<physx::PxRigidDynamic>())
	{
		// 키네마틱이면 0 반환
		if (m_Desc.isKinematic) return Vector3();
		return ToVec(d->getLinearVelocity());
	}
	return Vector3();
}

void MMMEngine::RigidBodyComponent::SetAngularVelocity(Vector3 w)
{
	if (!m_Actor) return;
	if (m_Desc.type != Type::Dynamic) return;
	if (m_Desc.isKinematic) return;

	if (auto* d = m_Actor->is<physx::PxRigidDynamic>())
	{
		d->setAngularVelocity(ToPxVec(w));
		d->wakeUp();
	}
}

Vector3 MMMEngine::RigidBodyComponent::GetAngularVelocity() const
{
	if (!m_Actor) return Vector3();
	if (m_Desc.type != Type::Dynamic) return Vector3();

	if (auto* d = m_Actor->is<physx::PxRigidDynamic>())
	{
		if (m_Desc.isKinematic) return Vector3();
		return ToVec(d->getAngularVelocity());
	}
	return Vector3();
}

void MMMEngine::RigidBodyComponent::SetisAutoRigid(bool value)
{
	m_IsAutoRigid = value;
}

bool MMMEngine::RigidBodyComponent::GetisAutoRigid()
{
	return m_IsAutoRigid;
}

void MMMEngine::RigidBodyComponent::WakeUp()
{
	m_WakeRequested = true;
}

physx::PxRigidActor* MMMEngine::RigidBodyComponent::GetPxActor() const
{
	return m_Actor;
}

void MMMEngine::RigidBodyComponent::AttachShapeOnly(physx::PxShape* shape)
{
	if (!m_Actor || !shape) return;
	m_Actor->attachShape(*shape);
}

void MMMEngine::RigidBodyComponent::SetType_Internal()
{
	m_Desc.type = m_RequestedType;
}

bool MMMEngine::RigidBodyComponent::HasPendingTypeChange()
{
	return m_TypeChangePending;
}

void MMMEngine::RigidBodyComponent::OffPendingType()
{
	m_TypeChangePending = false;
}



void MMMEngine::RigidBodyComponent::BindTeleport()
{
	auto Bind_Trans = GetGameObject();
	auto temptrans = Bind_Trans->GetTransform();
	SetKinematicTarget(temptrans->GetWorldPosition(), temptrans->GetWorldRotation());
}

//private 함수들
physx::PxForceMode::Enum MMMEngine::RigidBodyComponent::ToPxForceMode(ForceMode mode)
{
	switch (mode)
	{
	case ForceMode::Force: return physx::PxForceMode::eFORCE;
	case ForceMode::Impulse: return physx::PxForceMode::eIMPULSE;
	case ForceMode::VelocityChange: return physx::PxForceMode::eVELOCITY_CHANGE;
	case ForceMode::Acceleration: return physx::PxForceMode::eACCELERATION;
	}
	return physx::PxForceMode::eFORCE;
}

void MMMEngine::RigidBodyComponent::SetUseGravity(bool value)
{
	auto it  = MMMEngine::PhysxManager::Get().getPScene();


	if (&(it->GetScene()) == nullptr)
	{
		std::cout << "GetScene is nullptr" << std::endl;
	}
	/*physx::PxU32 count = it->GetScene().getNbActors(physx::PxActorTypeFlag::eRIGID_STATIC |
		physx::PxActorTypeFlag::eRIGID_DYNAMIC);

	std::cout << count << std::endl;*/
	m_Desc.useGravity = value; m_DescDirty = true; m_WakeRequested = true;
}

void MMMEngine::RigidBodyComponent::SetType(Type newType)
{
	if (m_Desc.type == newType)
		return;

	//현재 월드 포즈 확보
	Vector3 temp_Position = {};
	Quaternion temp_Quarter = {};


	if (m_Actor)
	{
		auto px = m_Actor->getGlobalPose();
		temp_Position = ToVec(px.p);
		temp_Quarter = ToQuat(px.q);
	}
	else if (auto tr = GetTransform())
	{
		temp_Position = tr->GetWorldPosition();
		temp_Quarter = tr->GetWorldRotation();
	}
	else
	{
		temp_Position = Vector3{ 0,0,0 };
		temp_Quarter = Quaternion::Identity;
	}

	m_RequestedType = newType;
	m_RequestedPos = temp_Position;
	m_RequestedRot = temp_Quarter;
	m_TypeChangePending = true;

	// wake는 '나중에' 씬에 들어간 뒤에 처리되게 플래그만
	m_WakeRequested = true;

	// 매니저에 처리 요청
	MMMEngine::PhysxManager::Get().NotifyRigidTypeChanged(this);
}
