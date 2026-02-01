#include "CapsuleColliderComponent.h"
#include "rttr/registration"
#include "PhysxManager.h"
#include "PhysxHelper.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<CapsuleColliderComponent>("CapsuleCollider")
		(rttr::metadata("wrapper_type_name", "ObjPtr<CapsuleColliderComponent>"))
		.property("Radius", &CapsuleColliderComponent::GetRadius, &CapsuleColliderComponent::SetRadius)
		.property("Height", &CapsuleColliderComponent::GetHalfHeight, &CapsuleColliderComponent::SetHalfHeight)
		.property("Center", &ColliderComponent::GetLocalCenter, &ColliderComponent::SetLocalCenter)
		;

	registration::class_<ObjPtr<CapsuleColliderComponent>>("ObjPtr<CapsuleColliderComponent>")
		.constructor(
			[]() {
				return Object::NewObject<CapsuleColliderComponent>();
			})
		.method("Inject", &ObjPtr<CapsuleColliderComponent>::Inject);
}

void MMMEngine::CapsuleColliderComponent::SetRadius(float radius)
{
	m_radius = radius; if (m_Shape) MarkGeometryDirty();
}

void MMMEngine::CapsuleColliderComponent::SetHalfHeight(float halfheight)
{
	m_halfHeight = halfheight; if (m_Shape) MarkGeometryDirty();
}

float MMMEngine::CapsuleColliderComponent::GetRadius() const
{
	return m_radius;
}

float MMMEngine::CapsuleColliderComponent::GetHalfHeight() const
{
	return m_halfHeight;
}

bool MMMEngine::CapsuleColliderComponent::UpdateShapeGeometry()
{
	if (!m_Shape)
		return false;

	physx::PxGeometryHolder holder = m_Shape->getGeometry();
	if (holder.getType() != physx::PxGeometryType::eCAPSULE)
		return false;

	const float r = physx::PxMax(m_radius, 0.01f);
	const float hh = physx::PxMax(m_halfHeight, 0.01f);

	physx::PxCapsuleGeometry geom(r, hh);
	if (!geom.isValid()) return false;
	m_Shape->setGeometry(geom);
	ApplyAll();
	return true;
}

void MMMEngine::CapsuleColliderComponent::ApplyLocalPose()
{
	if (!m_Shape) return;
	// rigidOffset -> local -> axisCorrection 순서
	m_Shape->setLocalPose(m_RigidOffsetPose * m_LocalPose * m_AxisCorrection);
}

void MMMEngine::CapsuleColliderComponent::BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material)
{
    if (!physics || !material) return;

    const float t_radius = (m_radius > 0.0f) ? m_radius : 0.01f;
    const float t_halfheight = (m_halfHeight > 0.0f) ? m_halfHeight : 0.01f;

    physx::PxCapsuleGeometry geom(t_radius, t_halfheight);

    // shape 생성
    physx::PxShape* shape = physics->createShape(geom, *material, true);
    if (!shape) return;

    // 부모가 userData 설정 및 ApplyAll 수행
    SetShape(shape, true);
}

MMMEngine::ColliderComponent::DebugColliderShapeDesc MMMEngine::CapsuleColliderComponent::GetDebugShapeDesc() const
{
	DebugColliderShapeDesc s_Desc;
	s_Desc.type = DebugColliderType::Capsule;
	s_Desc.radius = m_radius;
	s_Desc.halfHeight = m_halfHeight;

	s_Desc.localCenter = ToVec(m_LocalPose.p);
	s_Desc.localRotation = ToQuat(m_LocalPose.q);
	return s_Desc;
}

//void MMMEngine::ColliderComponent::Initialize()
//{
//	// Shape를 먼저 생성해야 AttachCollider에서 사용할 수 있음
//	auto& physics = MMMEngine::PhysicX::Get().GetPhysics();
//	physx::PxMaterial* mat = MMMEngine::PhysicX::Get().GetDefaultMaterial();
//
//	if (mat)
//	{
//		BuildShape(&physics, mat);
//	}
//	MMMEngine::PhysxManager::Get().NotifyColliderAdded(this);
//}


