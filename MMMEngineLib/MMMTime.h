#pragma once
#include "TimeManager.h"

namespace MMMEngine::Time
{
	inline float GetDeltaTime() { return TimeManager::Get().GetDeltaTime(); }
	inline float GetFixedDeltaTime() { return TimeManager::Get().GetFixedDeltaTime(); }
	inline float GetTotalTime() { return TimeManager::Get().GetTotalTime(); }
	inline float GetFixedTime() { return TimeManager::Get().GetFixedTime(); }
	inline float GetUnscaledTime() { return TimeManager::Get().GetUnscaledTime(); }
	inline float GetUnscaledDeltaTime() { return TimeManager::Get().GetUnscaledDeltaTime(); }
	inline float GetTimeScale() { return TimeManager::Get().GetTimeScale(); }
	inline float GetMaximumAllowedTimestep() { return TimeManager::Get().GetMaximumAllowedTimestep(); }
}