#include "SceneNameWindow.h"
#include "SceneManager.h"
#include "SceneChangeWindow.h"
#include "EditorRegistry.h"
#include "json/json.hpp"
using namespace MMMEngine::EditorRegistry;
using json = nlohmann::json;
#include "ProjectManager.h"
#include "SceneSerializer.h"

namespace MMMEngine::Editor
{
	void SceneChangeWindow::Render()
	{
		ImGuiWindowClass wc;
		wc.ParentViewportId = ImGui::GetMainViewport()->ID;
		wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
		ImGui::SetNextWindowClass(&wc);

		if (!g_editor_window_sceneChange)
			return;

		ImGuiViewport* vp = ImGui::GetMainViewport();
		ImVec2 center = vp->GetCenter();
		ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));

		static int selectedSceneIndex = -1;
		bool applyButtonEnabled = false;

		if (ImGui::Begin(u8"씬 전환", &g_editor_window_sceneChange,
			ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_AlwaysAutoResize))
		{
			auto& sceneManager = SceneManager::Get();
			const auto& scenesHash = sceneManager.GetScenesHash();

			// Get current scene info
			SceneRef currentSceneRef = sceneManager.GetCurrentScene();
			Scene* currentScene = sceneManager.GetSceneRaw(currentSceneRef);
			std::string currentSceneName = currentScene ? currentScene->GetName() : "None";

			// Display current scene
			ImGui::Text(u8"현재 씬: %s", currentSceneName.c_str());
			ImGui::Separator();
			ImGui::Spacing();

			// Create a sorted list of scenes for consistent display
			std::vector<std::pair<std::string, size_t>> sortedScenes;
			for (const auto& [name, id] : scenesHash)
			{
				sortedScenes.emplace_back(name, id);
			}
			std::sort(sortedScenes.begin(), sortedScenes.end(),
				[](const auto& a, const auto& b) { return a.second < b.second; });

			// Scene list
			ImGui::Text(u8"사용 가능한 씬:");
			ImGui::BeginChild("SceneList", ImVec2(400, 300), true);

			for (size_t i = 0; i < sortedScenes.size(); ++i)
			{
				const auto& [sceneName, sceneId] = sortedScenes[i];

				// Highlight current scene
				bool isCurrent = (sceneId == currentSceneRef.id);
				if (isCurrent)
				{
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.2f, 0.8f, 0.2f, 1.0f));
				}

				// Make selectable item
				char label[256];
				snprintf(label, sizeof(label), "[%zu] %s%s", sceneId, sceneName.c_str(),
					isCurrent ? u8" (현재)" : "");

				if (ImGui::Selectable(label, selectedSceneIndex == static_cast<int>(i)))
				{
					selectedSceneIndex = static_cast<int>(i);
				}

				if (isCurrent)
				{
					ImGui::PopStyleColor();
				}

				// Double click to change scene
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
				{
					if (!isCurrent)
					{
						sceneManager.ChangeScene(sceneId);
						g_editor_window_sceneChange = false;
					}
				}
			}

			ImGui::EndChild();

			ImGui::Spacing();

			// Display info about selected scene
			if (selectedSceneIndex >= 0 && selectedSceneIndex < static_cast<int>(sortedScenes.size()))
			{
				const auto& [selectedName, selectedId] = sortedScenes[selectedSceneIndex];
				applyButtonEnabled = (selectedId != currentSceneRef.id);

				ImGui::TextColored(ImVec4(0.7f, 0.7f, 0.7f, 1.0f),
					u8"선택된 씬: [%zu] %s", selectedId, selectedName.c_str());
			}
			else
			{
				ImGui::TextColored(ImVec4(0.5f, 0.5f, 0.5f, 1.0f),
					u8"씬을 선택하세요.");
			}

			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Buttons
			ImGui::BeginDisabled(!applyButtonEnabled);
			if (ImGui::Button(u8"씬 변경", ImVec2(190, 0)))
			{
				if (selectedSceneIndex >= 0 && selectedSceneIndex < static_cast<int>(sortedScenes.size()))
				{
					const auto& [selectedName, selectedId] = sortedScenes[selectedSceneIndex];
					sceneManager.ChangeScene(selectedId);
					g_editor_window_sceneChange = false;
					selectedSceneIndex = -1;
				}
			}
			ImGui::EndDisabled();

			ImGui::SameLine();

			if (ImGui::Button(u8"취소", ImVec2(190, 0)))
			{
				g_editor_window_sceneChange = false;
				selectedSceneIndex = -1;
			}

			ImGui::Spacing();

			// Help text
			ImGui::TextColored(ImVec4(0.6f, 0.6f, 0.6f, 1.0f),
				u8"팁: 씬을 더블클릭하여 빠르게 변경할 수 있습니다.");

			ImGui::End();
		}
		else
		{
			// Reset selection when window is closed
			selectedSceneIndex = -1;
		}
	}
}
