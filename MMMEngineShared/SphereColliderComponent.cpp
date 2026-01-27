#include "SphereColliderComponent.h"
#include "rttr/registration"
#include "PhysxManager.h"
#include "PhysxHelper.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;
	registration::class_<SphereColliderComponent>("SphereCollider")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<SphereColliderComponent>>()))
		.property("Radius", &SphereColliderComponent::GetRadius, &SphereColliderComponent::SetRadius)
		.property("Center", &ColliderComponent::GetLocalCenter, &ColliderComponent::SetLocalCenter)
		;

	registration::class_<ObjPtr<SphereColliderComponent>>("ObjPtr<SphereCollider>")
		.constructor(
			[]() {
				return Object::NewObject<SphereColliderComponent>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<SphereColliderComponent>>();
}

void MMMEngine::SphereColliderComponent::SetRadius(float radius)
{
	if (m_radius == radius) return;
	m_radius = radius;

	if (m_Shape) MarkGeometryDirty();
}

float MMMEngine::SphereColliderComponent::GetRadius() const
{
	return m_radius;
}

bool MMMEngine::SphereColliderComponent::UpdateShapeGeometry()
{
	if (!m_Shape) return false;

	physx::PxGeometryHolder holder = m_Shape->getGeometry();
	if (holder.getType() != physx::PxGeometryType::eSPHERE) return false;

	const float r = physx::PxMax(m_radius, 0.01f);

	physx::PxSphereGeometry geom(r);
	if (!geom.isValid()) return false;

	m_Shape->setGeometry(geom);
	ApplyAll(); 
	return true;
}

void MMMEngine::SphereColliderComponent::BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material)
{
	if (!physics || !material) return;

	const float r = (m_radius > 0.0f) ? m_radius : 0.01f;

	physx::PxSphereGeometry geom(r);

	physx::PxShape* shape = physics->createShape(geom, *material, true);
	if (!shape) return;

	SetShape(shape, true);
}

MMMEngine::ColliderComponent::DebugColliderShapeDesc MMMEngine::SphereColliderComponent::GetDebugShapeDesc() const
{
	DebugColliderShapeDesc s_Desc;
	s_Desc.type = DebugColliderType::Sphere;
	s_Desc.sphereRadius = m_radius;

	s_Desc.localCenter = ToVec(m_LocalPose.p);
	s_Desc.localRotation = ToQuat(m_LocalPose.q);
	return s_Desc;
}

