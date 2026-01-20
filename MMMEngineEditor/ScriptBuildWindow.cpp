#include "ScriptBuildWindow.h"
#include "BuildManager.h"
#include "ProjectManager.h"
#include "DLLHotLoadHelper.h"
#include "BehaviourManager.h"

#include <filesystem>

namespace fs = std::filesystem;

namespace MMMEngine::Editor
{
    void ScriptBuildWindow::StartBuild()
    {
        if (m_building.exchange(true))
            return;

        m_progress.store(0.f);
        m_exitCode.store(0);
        m_showToast = true;

        // 이전 스레드 정리
        if (m_worker.joinable())
            m_worker.join();

        m_worker = std::thread([this]()
            {
                auto& bm = BuildManager::Get();

                bm.SetProgressCallbackPercent([this](float p)
                    {
                        if (p < 0.f) p = 0.f;
                        if (p > 100.f) p = 100.f;
                        m_progress.store(p);
                    });

                const auto projectRoot =
                    ProjectManager::Get().GetActiveProject().rootPath;

                auto out = bm.BuildUserScripts(projectRoot, BuildConfiguration::Debug);

                m_exitCode.store(out.exitCode);
                m_building.store(false);

                // 완료 후 1.5초 표시
                m_hideTime = ImGui::GetTime() + 1.5;
            });
    }


    void ScriptBuildWindow::Render()
    {
        // 끝난 스레드 수거 (terminate 방지)
        if (!m_building.load() && m_worker.joinable())
        {
            m_worker.join();

            fs::path cwd = fs::current_path();
            DLLHotLoadHelper::CleanupHotReloadCopies(cwd);

            fs::path projectPath = ProjectManager::Get().GetActiveProject().rootPath;

            fs::path dllPath = DLLHotLoadHelper::CopyDllForHotReload(projectPath / "Binaries" / "Win64" / "UserScripts.dll", cwd);
           
            if (!dllPath.empty())
            {
                // todo : 현재 씬 바꾸기
                BehaviourManager::Get().ReloadUserScripts(dllPath.u8string());
            }
        }
  

        if (!m_showToast)
            return;

        // 완료 후 자동 숨김
        if (!m_building.load() && ImGui::GetTime() > m_hideTime)
        {
            m_showToast = false;
            return;
        }

        ImGuiWindowFlags flags =
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_NoFocusOnAppearing |
            ImGuiWindowFlags_NoNav;

        // 화면 중앙 계산
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImVec2 center(
            vp->WorkPos.x + vp->WorkSize.x * 0.5f,
            vp->WorkPos.y + vp->WorkSize.y * 0.5f
        );

        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.85f);

        if (ImGui::Begin("##ScriptBuildToast", nullptr, flags))
        {
            if (m_building.load())
            {
                ImGui::TextUnformatted(u8"스크립트 빌드 중...");
                ImGui::ProgressBar(
                    m_progress.load() / 100.f,
                    ImVec2(260, 0)
                );
            }
            else
            {
                ImGui::TextUnformatted(
                    m_exitCode.load() == 0
                    ? u8"빌드 성공!"
                    : u8"빌드 실패!"
                );
            }
        }
        ImGui::End();
    }
}
