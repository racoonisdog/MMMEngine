#pragma once
#include <imgui.h>
#include <string>
#include <vector>
#include <filesystem>
#include <fstream>
#include <algorithm>
#include "Singleton.hpp"

namespace fs = std::filesystem;

namespace MMMEngine::Editor
{
    class FilesWindow : public Utility::Singleton<FilesWindow>
    {
    public:
        std::string selectedFileOrDir;
        std::string hoveredFileOrDir;
        bool hovered = false;

    private:
        std::vector<std::pair<std::string, std::string>> m_moveQueue;
        std::vector<std::string> m_fileRenderExclusions = { ".guid", ".csproj", ".vcxproj", ".filters", ".user" };
        std::vector<std::string> m_folderRenderExclusions = { "bin", "obj", "Build", "Binaries", ".vs" };

        char m_newScriptName[256] = "";
        bool m_openCreateScriptModalRequest = false;
        std::string m_newScriptParentDirectory;

        fs::path m_currentDirectory; // 현재 보고 있는 디렉토리
        std::vector<fs::path> m_navigationHistory; // 네비게이션 히스토리
        int m_historyIndex = -1;

        // 그리드 설정
        float m_iconSize = 80.0f;
        float m_iconPadding = 10.0f;

    public:
        void Render();

    private:
        void DrawToolbar(const fs::path& root);

        void DrawNavigationBar(const fs::path& root);

        void DrawGridView(const fs::path& root);

        void DrawGridItem(const fs::path& path, bool isDirectory, int index, int columns);

        void NavigateTo(const fs::path& newPath);

        void CreateNewScript(const std::string& parentDir, const std::string& scriptName);

        void OpenFileInEditor(const fs::path& filePath);

        fs::path MakeFileUnique(const fs::path& parentDir, const std::string& fileName, const std::string& extension);

        fs::path MakeFolderUnique(const fs::path& parentDir, const std::string& folderName);
    };
}