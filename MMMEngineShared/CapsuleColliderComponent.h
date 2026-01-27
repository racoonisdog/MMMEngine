#pragma once
#include "ColliderComponent.h"

namespace MMMEngine
{
	class MMMENGINE_API CapsuleColliderComponent : public ColliderComponent
	{
	private:
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
	public:
		void SetRadius(float radius);
		void SetHalfHeight(float halfheight);

		float GetRadius() const;
		float GetHalfHeight() const;

		bool UpdateShapeGeometry() override;

		void BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material) override;

		DebugColliderShapeDesc GetDebugShapeDesc() const override;
	private:
		float m_radius = 0.5f;
		float m_halfHeight = 1.0f;
	};
}

