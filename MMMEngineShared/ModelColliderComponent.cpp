#include "ModelColliderComponent.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<ModelColliderComponent>("ModleCollider")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<ModelColliderComponent>>()));

	registration::class_<ObjPtr<ModelColliderComponent>>("ObjPtr<ModleCollider>")
		.constructor(
			[]() {
				return Object::NewObject<ModelColliderComponent>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<ModelColliderComponent>>();
}

void MMMEngine::ModelColliderComponent::SetRadius(float radius)
{
	m_radius = radius; if (m_Shape) ApplyAll();
}

void MMMEngine::ModelColliderComponent::SetHalfHeight(float halfheight)
{
	m_halfHeight = halfheight; if (m_Shape) ApplyAll();
}

float MMMEngine::ModelColliderComponent::GetRadius() const
{
	return m_radius;
}

float MMMEngine::ModelColliderComponent::GetHalfHeight() const
{
	return m_halfHeight;
}

void MMMEngine::ModelColliderComponent::BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material)
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

	// Y-up 캡슐이 필요하면 아래코드로
	// SetLocalPose( physx::PxTransform(physx::PxQuat(physx::PxHalfPi, physx::PxVec3(0,0,1))) * m_LocalPose );
}