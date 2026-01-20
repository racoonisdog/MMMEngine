#include "RigidBodyComponent.h"
#include "PhysXHelper.h"
#include "Transform.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<RigidBodyComponent>("RigidBodyComponent")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<RigidBodyComponent>>()));

	registration::class_<ObjPtr<RigidBodyComponent>>("ObjPtr<RigidBodyComponent>")
		.constructor(
			[]() {
				return Object::NewObject<RigidBodyComponent>();
			});

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
}

void MMMEngine::RigidBodyComponent::OnDestroy()
{
	//for (auto* col : m_Colliders) DetachCollider(col);

	auto cols = m_Colliders;
	for (auto* col : cols) DetachCollider(col);
	DestroyActor();

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

void MMMEngine::RigidBodyComponent::WakeUp()
{
	m_WakeRequested = true;
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

void MMMEngine::RigidBodyComponent::SetType(Type newType)
{
	if (m_Desc.type == newType)
		return;

	//현재 월드 포즈 확보
	Vector3 temp_Position = {};
	Quaternion temp_Quarter = {};


	if (m_Actor)
	{
		if (auto* t_dynamic = m_Actor->is<physx::PxRigidDynamic>())
		{
			auto px = t_dynamic->getGlobalPose();
			temp_Position = ToVec(px.p);
			temp_Quarter = ToQuat(px.q);
		}
		else
		{
			auto px = m_Actor->getGlobalPose();
			temp_Position = ToVec(px.p);
			temp_Quarter = ToQuat(px.q);
		}
	}
	else
	{
		if (auto tr = GetTransform())
		{
			temp_Position = tr->GetWorldPosition();
			temp_Quarter = tr->GetWorldRotation();
		}
		else
		{
			temp_Position = Vector3{ 0,0,0 };
			temp_Quarter = Quaternion::Identity;
		}
	}

	//타입 변경
	m_Desc.type = newType;

	//Actor 재생성
	DestroyActor();
	CreateActor(m_Physics, temp_Position, temp_Quarter);
}
