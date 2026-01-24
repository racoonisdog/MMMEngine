#include "BoxColliderComponent.h"
#include "rttr/registration"
#include "PhysxManager.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<BoxColliderComponent>("BoxCollider")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<BoxColliderComponent>>()))
		.property("Extents", &BoxColliderComponent::GetHalfExtents, &BoxColliderComponent::SetHalfExtents)
		.property("Center", &ColliderComponent::GetLocalCenter, &ColliderComponent::SetLocalCenter)
		;

	registration::class_<ObjPtr<BoxColliderComponent>>("ObjPtr<BoxCollider>")
		.constructor(
			[]() {
				return Object::NewObject<BoxColliderComponent>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<BoxColliderComponent>>();
}

void MMMEngine::BoxColliderComponent::SetHalfExtents(Vector3 he)
{
	{ m_halfExtents = he; if (m_Shape) MarkGeometryDirty(); }
}

Vector3 MMMEngine::BoxColliderComponent::GetHalfExtents() const
{
	return m_halfExtents;
}

bool MMMEngine::BoxColliderComponent::UpdateShapeGeometry()
{
	if (!m_Shape) return false;

	physx::PxGeometryHolder holder = m_Shape->getGeometry();
	if (holder.getType() != physx::PxGeometryType::eBOX) return false;

	const float hx = physx::PxMax(m_halfExtents.x, 0.01f);
	const float hy = physx::PxMax(m_halfExtents.y, 0.01f);
	const float hz = physx::PxMax(m_halfExtents.z, 0.01f);

	physx::PxBoxGeometry geom(hx, hy, hz);
	if (!geom.isValid())
		return false;

	m_Shape->setGeometry(geom);
	ApplyAll();
	return true;
}

void MMMEngine::BoxColliderComponent::BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material)
{
	if (!physics || !material) return;

	const float hx = (m_halfExtents.x > 0.f) ? m_halfExtents.x : 0.01f;
	const float hy = (m_halfExtents.y > 0.f) ? m_halfExtents.y : 0.01f;
	const float hz = (m_halfExtents.z > 0.f) ? m_halfExtents.z : 0.01f;

	physx::PxBoxGeometry geom(hx, hy, hz);

	physx::PxShape* shape = physics->createShape(geom, *material, true);
	if (!shape) return;

	SetShape(shape, true);
}