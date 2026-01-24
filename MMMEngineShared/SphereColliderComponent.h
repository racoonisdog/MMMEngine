#pragma once
#include "ColliderComponent.h"

namespace MMMEngine
{
	class MMMENGINE_API SphereColliderComponent : public ColliderComponent
	{
	private:
		RTTR_ENABLE(Component)
			RTTR_REGISTRATION_FRIEND
	public:
		void SetRadius(float radius);

		float GetRadius() const;

		bool UpdateShapeGeometry() override;

		void BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material) override;
	private:
		float m_radius = 0.5f;
	};
}


