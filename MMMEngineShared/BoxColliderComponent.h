#pragma once
#include "ColliderComponent.h"

namespace MMMEngine
{
	class MMMENGINE_API BoxColliderComponent : public ColliderComponent
	{
	private:
		RTTR_ENABLE(Component)
			RTTR_REGISTRATION_FRIEND
	public:
		void SetHalfExtents(Vector3 he);
		Vector3 GetHalfExtents() const;

		bool UpdateShapeGeometry() override;

		void BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material) override;
	private:
		Vector3 m_halfExtents = { 0.5f, 0.5f, 0.5f };
	};
}