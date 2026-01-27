#include "ScriptBuildWindow.h"
#include "BuildManager.h"
#include "ProjectManager.h"
#include "DLLHotLoadHelper.h"
#include "BehaviourManager.h"
#include "RenderManager.h"
#include "ShaderInfo.h"
#include "SceneManager.h"
#include "SceneSerializer.h"
#include "StringHelper.h"
#include "EditorRegistry.h"
#include "InspectorWindow.h"

#include <filesystem>

namespace fs = std::filesystem;
using namespace MMMEngine::Utility;

namespace MMMEngine::Editor
{
    void ScriptBuildWindow::StartBuild()
    {
        if (m_building.exchange(true))
            return;

        m_progress.store(0.f);
        m_exitCode.store(0);
        m_showToast = true;

        fs::path projectPath = ProjectManager::Get().GetActiveProject().rootPath;
        fs::path binDir = projectPath / "Binaries" / "Win64";
        DLLHotLoadHelper::BackupOriginalDll(binDir);
        DLLHotLoadHelper::PrepareForBuild(binDir);
        {
            auto sceneRef = SceneManager::Get().GetCurrentScene();
            auto sceneRaw = SceneManager::Get().GetSceneRaw(sceneRef);

            SceneSerializer::Get().Serialize(*sceneRaw, SceneManager::Get().GetSceneListPath() + L"/" +
            StringHelper::StringToWString(sceneRaw->GetName()) + L".scene");
            SceneSerializer::Get().ExtractScenesList(SceneManager::Get().GetAllSceneToRaw(), SceneManager::Get().GetSceneListPath());

            EditorRegistry::g_selectedGameObject = nullptr;
            InspectorWindow::Get().ClearCache();

            RenderManager::Get().ClearAllCommands();

            SceneManager::Get().ShutDown();
            ObjectManager::Get().ShutDown();

            BehaviourManager::Get().UnloadUserScripts();
        }

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

#ifdef _DEBUG
                auto out = bm.BuildUserScripts(projectRoot, BuildConfiguration::Debug);
#else
                auto out = bm.BuildUserScripts(projectRoot, BuildConfiguration::Release);
#endif

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
            m_worker.join(); // 한 번만 호출

            fs::path cwd = fs::current_path();
            DLLHotLoadHelper::CleanupHotReloadCopies(cwd);

            fs::path projectPath = ProjectManager::Get().GetActiveProject().rootPath;
            fs::path binDir = projectPath / "Binaries" / "Win64";
            fs::path originDllPath = binDir / "UserScripts.dll";

            // [방어 기제] 빌드 결과에 따른 분기
            if (m_exitCode.load() != 0)
            {
                // 빌드 실패: 원본 복구
                DLLHotLoadHelper::RestoreOriginalDll(binDir);

                // 복구된 DLL을 핫로드용 폴더로 복사
                fs::path restoredPath = DLLHotLoadHelper::CopyDllForHotReload(originDllPath, cwd);
                if (!restoredPath.empty())
                {
                    // 이전 상태로 복구 리로드 (StartUp은 생략하거나 필요시 호출)
                    BehaviourManager::Get().ReloadUserScripts(restoredPath.stem().u8string());

                    // 빌드 전 ShutDown을 했으므로, 엔진을 다시 가동시켜야 함
                    auto currentProject = ProjectManager::Get().GetActiveProject();
                    SceneManager::Get().StartUp(currentProject.ProjectRootFS().generic_wstring() + L"/Assets/Scenes", currentProject.lastSceneIndex, true);
                    ObjectManager::Get().StartUp();
                }
                return;
            }

            // 빌드 성공: 백업 제거
            std::error_code ec;
            fs::remove(binDir / "UserScripts.dll.bak", ec);
            fs::remove(binDir / "UserScripts.pdb.bak", ec);

            // 새로운 DLL 복사 및 리로드
            fs::path dllPath = DLLHotLoadHelper::CopyDllForHotReload(originDllPath, cwd);
            if (!dllPath.empty())
            {
                bool unloadSuccess = BehaviourManager::Get().ReloadUserScripts(dllPath.stem().u8string());

                if (unloadSuccess)
                {
                    auto currentProject = ProjectManager::Get().GetActiveProject();
                    SceneManager::Get().StartUp(currentProject.ProjectRootFS().generic_wstring() + L"/Assets/Scenes", currentProject.lastSceneIndex, true);
                    ObjectManager::Get().StartUp();
                    SceneManager::Get().CheckSceneIsChanged();
                }
                else
                {
                    // RTTR 언로드 실패 시 대응 (로그 출력 및 사용자 알림)
                    // 이 상태에서 StartUp을 강제하면 충돌 위험이 큼
                    // todo : 에러 로그 남기고, 에러 로그가 있는 경우 프로젝트 매니저에서 안전모드 실행시키기
                }
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
