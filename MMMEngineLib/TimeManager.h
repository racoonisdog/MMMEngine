#pragma once
#include "Singleton.hpp"
#include <chrono>

using namespace MMMEngine::Utility;

namespace MMMEngine
{
	class TimeManager : public Singleton<TimeManager>
	{
	private:
		std::chrono::steady_clock::time_point m_initTime;
		std::chrono::steady_clock::time_point m_prevTime;
		std::chrono::steady_clock::time_point m_currentTime;
		std::chrono::steady_clock::duration m_deltaDuration;
		std::chrono::steady_clock::duration m_totalDuration;

		float m_deltaTime = 0;
		float m_fixedDeltaTime = 0.03125f;
		float m_totalTime = 0;
		float m_fixedTime = 0;
		float m_maximumAllowedTimestep = 0.2f;
		float m_timeScale = 1.0f;
		float m_unscaledDeltaTime = 0;
		float m_unscaledTotalTime = 0;
		float m_accumulator = 0;
		int m_fixedStepsThisFrame = 0;
		float m_interpolationAlpha = 0;
		uint32_t m_frameCount = 0;

	public:
		void StartUp();
		void ShutDown();
		void BeginFrame();

		template<typename Fn>
		void ConsumeFixedSteps(Fn&& step)
		{
			for (int i = 0; i < m_fixedStepsThisFrame; ++i)
			{
				m_fixedTime += m_fixedDeltaTime;
				step(m_fixedDeltaTime);
			}
		}
		
		const float GetDeltaTime() const { return m_deltaTime; }
		const float GetFixedDeltaTime() const { return m_fixedDeltaTime; }
		const float GetTotalTime() const { return m_totalTime; }
		const float GetFixedTime() const { return m_fixedTime; }
		const float GetUnscaledTime() const { return m_unscaledTotalTime; }
		const float GetUnscaledDeltaTime() const { return m_unscaledDeltaTime; }
		const float GetTimeScale() const { return m_timeScale; }
		const float GetMaximumAllowedTimestep() const { return m_maximumAllowedTimestep; }
		const uint32_t GetFrameCount() const { return m_frameCount; }
	
		void SetFixedDeltaTime(float fixedDelta) { m_fixedDeltaTime = fixedDelta; }
		void SetMaximumAllowedTimestep(float allowedTimestep) { m_maximumAllowedTimestep = allowedTimestep; }

		void SetDefaultFixedDeltaTime() { m_fixedDeltaTime = 0.03125f; }
		void SetDefaultMaximumAllowedTimestep() { m_maximumAllowedTimestep = 0.2f; }
	};
}