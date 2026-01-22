#include "ConsoleWindow.h"
#include <cstdarg>
#include <cstdio>
#include <iostream>
#include "EditorRegistry.h"

using namespace MMMEngine::EditorRegistry;

namespace MMMEngine::Editor
{
    void ConsoleWindow::Init()
    {
        m_Buffer.reserve(64 * 1024);

        // cout/cerr 가로채기 설치
        m_HookBuf = std::make_unique<ConsoleStreamBuf>(
            [this](const std::string& line)
            {
                this->AddLog(line); // 이미 mutex 잡는 AddLog 사용
            });

        m_OldCout = std::cout.rdbuf(m_HookBuf.get());
        m_OldCerr = std::cerr.rdbuf(m_HookBuf.get());

        // 버퍼링 때문에 바로 안 보이면 이거도 추천
        std::cout.setf(std::ios::unitbuf);
        std::cerr.setf(std::ios::unitbuf);
    }

    void ConsoleWindow::Shutdown()
    {
        if (m_OldCout) std::cout.rdbuf(m_OldCout);
        if (m_OldCerr) std::cerr.rdbuf(m_OldCerr);

        m_HookBuf.reset();
        m_OldCout = nullptr;
        m_OldCerr = nullptr;
    }

    void ConsoleWindow::Clear()
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Buffer.clear();
        m_ScrollToBottom = true; // 비운 뒤 아래로 맞추고 싶으면
    }

    void ConsoleWindow::AddLog(const std::string& msg)
    {
        std::lock_guard<std::mutex> lock(m_Mutex);
        m_Buffer.append(msg);
        m_Buffer.push_back('\n');
        if(m_AutoScroll)
            m_ScrollToBottom = true;
    }

    void ConsoleWindow::AddLog(const char* fmt, ...)
    {
        char temp[2048];

        va_list args;
        va_start(args, fmt);
        vsnprintf(temp, sizeof(temp), fmt, args);
        va_end(args);

        AddLog(std::string(temp));
    }

    int ConsoleWindow::InputTextCallback(ImGuiInputTextCallbackData* data)
    {
        if (data->EventFlag == ImGuiInputTextFlags_CallbackResize)
        {
            // data->UserData == std::string*
            auto* buffer = static_cast<std::string*>(data->UserData);

            // Resize string to new buffer size
            buffer->resize(data->BufTextLen);

            // IMPORTANT: update Buf pointer
            data->Buf = buffer->data();
        }
        return 0;
    }

    void ConsoleWindow::Render()
    {
        if (!g_editor_window_console)
            return;

        ImGuiWindowClass wc;
        wc.ParentViewportId = ImGui::GetMainViewport()->ID;
        wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing; // 필요 시 설정

        ImGui::SetNextWindowClass(&wc);

        static const char* ICON_FOLDER = "\xef\x84\xa0";

        std::string title = std::string(ICON_FOLDER) + u8" 콘솔";

        ImGui::Begin(title.c_str(), &g_editor_window_console);

        ImGui::Checkbox(u8"자동 스크롤", &m_AutoScroll);
        ImGui::SameLine();
        if (ImGui::Button(u8"비우기"))
            Clear();
        ImGui::SameLine();
        if (ImGui::Button(u8"전체 복사"))
        {
            // 콘솔 전체를 클립보드로
            ImGui::LogToClipboard();
            ImGui::LogText("%s", m_Buffer.c_str());
            ImGui::LogFinish();
        }

        ImGui::Separator();

        ImGui::BeginChild("ConsoleScrollRegion", ImVec2(0, 0), false,
            ImGuiWindowFlags_HorizontalScrollbar);

        bool shouldScroll = false;

        {
            std::lock_guard<std::mutex> lock(m_Mutex);

            // 텍스트 출력 (이건 스크롤을 만들지 않음. 스크롤은 Child가 담당)
            ImGui::TextUnformatted(m_Buffer.c_str());

            shouldScroll = m_ScrollToBottom;
            m_ScrollToBottom = false;
        }

        if (shouldScroll || (m_AutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f))
            ImGui::SetScrollHereY(1.0f);

        ImGui::EndChild();
        ImGui::End();
    }
}
