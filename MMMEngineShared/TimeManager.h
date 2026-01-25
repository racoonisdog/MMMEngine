#pragma once
#include "ExportSingleton.hpp"
#include <chrono>

// 아래는 표준라이브러리 (std::)의 dll export경고를 없애기 위한코드 
// EngineShared는 ABI가 유지됨이 보장되기때문에 4251경고에 대해 안전함
#pragma warning(push)
#pragma warning(disable: 4251)

namespace MMMEngine
{
	class MMMENGINE_API TimeManager : public Utility::ExportSingleton<TimeManager>
	{
	private:
		std::chrono::steady_clock::time_point m_initTime;
		std::chrono::steady_clock::time_point m_prevTime;
		std::chrono::steady_clock::time_point m_currentTime;
		std::chrono::steady_clock::duration m_deltaDuration = {};
		std::chrono::steady_clock::duration m_totalDuration = {};

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

		void ResetFixedStepAccumed();

		const float GetDeltaTime() const;
		const float GetFixedDeltaTime() const;
		const float GetTotalTime() const;
		const float GetFixedTime() const;
		const float GetUnscaledTime() const;
		const float GetUnscaledDeltaTime() const;
		const float GetTimeScale() const;
		const float GetMaximumAllowedTimestep() const;
		const uint32_t GetFrameCount() const;
	
		void SetFixedDeltaTime(float fixedDelta);
		void SetMaximumAllowedTimestep(float allowedTimestep);

		void SetDefaultFixedDeltaTime();
		void SetDefaultMaximumAllowedTimestep();

		void ResetFrameCount();
	};
}
#pragma warning(pop)