#pragma once
#include <imgui.h>
#include <memory>
#include <mutex>
#include <iostream>
#include <string>
#include "Singleton.hpp"
#include "ConsoleStreamBuf.h"

namespace MMMEngine::Editor
{
    class ConsoleWindow : public Utility::Singleton<ConsoleWindow>
    {
    public:
        void Init();
        void Shutdown();

        void AddLog(const char* fmt, ...);
        void AddLog(const std::string& msg);

        void Clear();
        void Render();

    private:
        static int InputTextCallback(ImGuiInputTextCallbackData* data);

    private:
        std::unique_ptr<ConsoleStreamBuf> m_HookBuf;
        std::streambuf* m_OldCout = nullptr;
        std::streambuf* m_OldCerr = nullptr;

        std::string m_Buffer;
        bool m_AutoScroll = true;
        bool m_ScrollToBottom = false;

        std::mutex m_Mutex;
    };
}