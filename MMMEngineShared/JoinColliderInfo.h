#pragma once
#include "Component.h"
#include "ColliderComponent.h"

namespace MMMEngine
{
	class MMMENGINE_API JoinColliderInfo : public MMMEngine::Component
	{
	public:
		void Initialize() override;
		void UnInitialize() override;

		MMMEngine::ColliderComponent* GetPoint();

	private:
		MMMEngine::ColliderComponent* ColliderPoint = nullptr;
	};
}

