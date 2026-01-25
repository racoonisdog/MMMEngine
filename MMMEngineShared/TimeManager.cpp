#include "TimeManager.h"
#include <Windows.h>

DEFINE_SINGLETON(MMMEngine::TimeManager)

using STD_Clock = std::chrono::steady_clock;

void MMMEngine::TimeManager::BeginFrame()
{
    m_frameCount++;

    m_currentTime = STD_Clock::now();
    auto delta = m_currentTime - m_prevTime;

    m_unscaledDeltaTime = std::chrono::duration<float>(delta).count();
    // clamp (스파이크 방지)
    if (m_unscaledDeltaTime > m_maximumAllowedTimestep)
        m_unscaledDeltaTime = m_maximumAllowedTimestep;

    m_deltaTime = m_unscaledDeltaTime * m_timeScale;

    auto total = m_currentTime - m_initTime;
    m_unscaledTotalTime = std::chrono::duration<float>(total).count();
    m_totalTime = m_unscaledTotalTime * m_timeScale;

    m_prevTime = m_currentTime;

    // fixed 스케줄링
    m_accumulator += m_unscaledDeltaTime;

    // 이번 프레임 fixed step 횟수 계산
    m_fixedStepsThisFrame = 0;
    while (m_accumulator >= m_fixedDeltaTime)
    {
        m_accumulator -= m_fixedDeltaTime;
        m_fixedStepsThisFrame++;
    }

    // 렌더 보간용 (0~1)
    m_interpolationAlpha = (m_fixedDeltaTime > 0.0f)
        ? (m_accumulator / m_fixedDeltaTime)
        : 0.0f;
}

void MMMEngine::TimeManager::ResetFixedStepAccumed()
{
    m_fixedStepsThisFrame = 0;
}

const float MMMEngine::TimeManager::GetDeltaTime() const
{
    return m_deltaTime;
}

const float MMMEngine::TimeManager::GetFixedDeltaTime() const
{
    return m_fixedDeltaTime;
}

const float MMMEngine::TimeManager::GetTotalTime() const
{
    return m_totalTime;
}

const float MMMEngine::TimeManager::GetFixedTime() const
{
    return m_fixedTime;
}

const float MMMEngine::TimeManager::GetUnscaledTime() const
{
    return m_unscaledTotalTime;
}

const float MMMEngine::TimeManager::GetUnscaledDeltaTime() const
{
    return m_unscaledDeltaTime;
}

const float MMMEngine::TimeManager::GetTimeScale() const
{
    return m_timeScale;
}

const float MMMEngine::TimeManager::GetMaximumAllowedTimestep() const
{
    return m_maximumAllowedTimestep;
}

const uint32_t MMMEngine::TimeManager::GetFrameCount() const
{
    return m_frameCount;
}

void MMMEngine::TimeManager::SetFixedDeltaTime(float fixedDelta)
{
    m_fixedDeltaTime = fixedDelta;
}

void MMMEngine::TimeManager::SetMaximumAllowedTimestep(float allowedTimestep)
{
    m_maximumAllowedTimestep = allowedTimestep;
}

void MMMEngine::TimeManager::SetDefaultFixedDeltaTime()
{
    m_fixedDeltaTime = 0.03125f;
}

void MMMEngine::TimeManager::SetDefaultMaximumAllowedTimestep()
{
    m_maximumAllowedTimestep = 0.2f;
}

void MMMEngine::TimeManager::ResetFrameCount()
{
    m_frameCount = 0;
}

void MMMEngine::TimeManager::StartUp()
{
    m_initTime = STD_Clock::now();
    m_prevTime = m_initTime;
    m_accumulator = 0.0f;
    m_fixedTime = 0.0f;
    m_frameCount = 0;
}

void MMMEngine::TimeManager::ShutDown()
{

}