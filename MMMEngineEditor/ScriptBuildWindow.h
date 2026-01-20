#pragma once
#include <imgui.h>
#include <atomic>
#include <mutex>
#include <deque>
#include <string>
#include <thread>

#include "Singleton.hpp"

namespace MMMEngine::Editor
{
    class ScriptBuildWindow : public Utility::Singleton<ScriptBuildWindow>
    {
    public:
        void StartBuild();
        void Render(); // 매 프레임 호출

    private:
        std::thread m_worker;

        std::atomic<bool>  m_building{ false };
        std::atomic<float> m_progress{ 0.f };
        std::atomic<int>   m_exitCode{ 0 };

        bool   m_showToast = false;
        double m_hideTime = 0.0; // 완료 후 자동 숨김 시간
    };

}
