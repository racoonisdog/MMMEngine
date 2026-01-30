#define NOMINMAX
#include "AssimpLoaderWindow.h"

#include "AssimpLoader.h"
#include "EditorRegistry.h"
#include "ProjectManager.h"
#include "StringHelper.h"

#include <imgui.h>
#include <filesystem>
#include <windows.h>
#include <commdlg.h>

using namespace MMMEngine;
using namespace MMMEngine::Editor;
using namespace MMMEngine::EditorRegistry;
using namespace MMMEngine::Utility;

namespace
{
	std::string GetExecutablePath()
	{
		char buffer[MAX_PATH];
		GetModuleFileNameA(NULL, buffer, MAX_PATH);
		std::filesystem::path exePath(buffer);
		return exePath.parent_path().string();
	}

	std::string OpenModelFileDialog(const std::string& initialDir)
	{
		OPENFILENAMEA ofn;
		char szFile[MAX_PATH] = { 0 };

		ZeroMemory(&ofn, sizeof(ofn));
		ofn.lStructSize = sizeof(ofn);
		ofn.hwndOwner = NULL;
		ofn.lpstrFile = szFile;
		ofn.nMaxFile = sizeof(szFile);
		ofn.lpstrFilter =
			"Model Files (*.fbx;*.obj;*.gltf;*.glb;*.dae)\0*.fbx;*.obj;*.gltf;*.glb;*.dae\0"
			"All Files (*.*)\0*.*\0";
		ofn.nFilterIndex = 1;
		ofn.lpstrFileTitle = NULL;
		ofn.nMaxFileTitle = 0;
		ofn.lpstrInitialDir = initialDir.empty() ? NULL : initialDir.c_str();
		ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST | OFN_NOCHANGEDIR;

		if (GetOpenFileNameA(&ofn) == TRUE)
		{
			return std::string(ofn.lpstrFile);
		}

		return {};
	}
}

void MMMEngine::Editor::AssimpLoaderWindow::Render()
{
	if (!g_editor_window_assimpLoader)
		return;

	ImGuiWindowClass wc;
	wc.ParentViewportId = ImGui::GetMainViewport()->ID;
	wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
	ImGui::SetNextWindowClass(&wc);

	if (ImGui::Begin(u8"Assimp 로더", &g_editor_window_assimpLoader, ImGuiWindowFlags_NoDocking))
	{
		static bool initialized = false;
		static char importPath[MAX_PATH] = "";
		static char exportPath[256] = "Assets/";
		static int modelType = 0;
		static char statusMsg[256] = "";

		auto& loader = AssimpLoader::Get();

		if (!initialized)
		{
			std::string exportPathStr = StringHelper::WStringToString(loader.m_exportPath);
			if (!exportPathStr.empty())
			{
				strncpy_s(exportPath, exportPathStr.c_str(), sizeof(exportPath) - 1);
				exportPath[sizeof(exportPath) - 1] = '\0';
			}
			initialized = true;
		}

		ImGui::Text(u8"모델 파일");
		ImGui::InputText("##assimp_import_path", importPath, sizeof(importPath));
		ImGui::SameLine();
		if (ImGui::Button(u8"찾아보기"))
		{
			std::string initialDir;
			if (ProjectManager::Get().HasActiveProject())
			{
				auto root = ProjectManager::Get().GetActiveProject().ProjectRootFS();
				initialDir = root.string();
			}
			else
			{
				initialDir = GetExecutablePath();
			}

			std::string selected = OpenModelFileDialog(initialDir);
			if (!selected.empty())
			{
				strncpy_s(importPath, selected.c_str(), sizeof(importPath) - 1);
				importPath[sizeof(importPath) - 1] = '\0';
			}
		}

		ImGui::Spacing();
		ImGui::Text(u8"모델 타입");
		ImGui::RadioButton(u8"정적", &modelType, 0);
		ImGui::SameLine();
		ImGui::RadioButton(u8"애니메이션", &modelType, 1);

		ImGui::Spacing();
		ImGui::Text(u8"내보내기 경로 (프로젝트 기준)");
		ImGui::InputText("##assimp_export_path", exportPath, sizeof(exportPath));

		ImGui::Spacing();
		if (ImGui::Button(u8"임포트"))
		{
			if (importPath[0] != '\0')
			{
				statusMsg[0] = '\0';

				std::filesystem::path importPathFs = std::filesystem::path(importPath).lexically_normal();
				std::filesystem::path pathForAssimp = importPathFs;

				if (importPathFs.is_absolute() || importPathFs.has_root_name())
				{
					// 절대경로는 프로젝트 루트 기준 상대경로로 변환해야 함
					auto root = ProjectManager::Get().HasActiveProject()
						? ProjectManager::Get().GetActiveProject().ProjectRootFS()
						: std::filesystem::path();

					if (root.empty())
					{
						strncpy_s(statusMsg, u8"프로젝트가 열려있지 않아 절대경로를 처리할 수 없습니다.", sizeof(statusMsg) - 1);
						statusMsg[sizeof(statusMsg) - 1] = '\0';
						return;
					}

					root = root.lexically_normal();
					std::error_code ec;
					std::filesystem::path rel = std::filesystem::relative(importPathFs, root, ec);
					if (ec || rel.empty())
					{
						strncpy_s(statusMsg, u8"프로젝트 루트 기준 상대경로 변환 실패.", sizeof(statusMsg) - 1);
						statusMsg[sizeof(statusMsg) - 1] = '\0';
						return;
					}

					auto it = rel.begin();
					if (it != rel.end() && *it == "..")
					{
						strncpy_s(statusMsg, u8"프로젝트 루트 내부 파일만 임포트 가능합니다.", sizeof(statusMsg) - 1);
						statusMsg[sizeof(statusMsg) - 1] = '\0';
						return;
					}

					pathForAssimp = rel;
				}

				loader.m_exportPath = StringHelper::StringToWString(exportPath);
				ModelType type = (modelType == 0) ? ModelType::Static : ModelType::Animated;
				loader.RegisterModel(pathForAssimp.wstring(), type);
			}
		}

		if (statusMsg[0] != '\0')
		{
			ImGui::Spacing();
			ImGui::TextColored(ImVec4(1.0f, 0.45f, 0.45f, 1.0f), "%s", statusMsg);
		}
	}
	ImGui::End();
}
