#include "pch.h"
#include "JoinColliderInfo.h"
#include "GameObject.h"

void MMMEngine::JoinColliderInfo::Initialize()
{
	auto it = GetGameObject()->GetComponent<ColliderComponent>().GetRaw();
	ColliderPoint = static_cast<ColliderComponent*>(it);
}

void MMMEngine::JoinColliderInfo::UnInitialize()
{
	ColliderPoint = nullptr;
}

MMMEngine::ColliderComponent* MMMEngine::JoinColliderInfo::GetPoint()
{
	return ColliderPoint;
}
