#pragma once

#include "Singleton.hpp"
#include "Project.h"

#include <optional>
#include <filesystem>

namespace MMMEngine::Editor
{
    class ProjectManager : public Utility::Singleton<ProjectManager>
    {
    public:
        // 에디터 시작 시
        bool Boot(); // 참 반환 시 프로젝트는 준비된 상태

        bool HasActiveProject() const;
        const Project& GetActiveProject() const;

        // Project operations
        bool OpenProject(const std::filesystem::path& projectFile);
        bool CreateNewProject(const std::filesystem::path& projectRootDir);

        // 저장 경로: projectRoot/ProjectSettings/project.json
        std::optional<std::filesystem::path> SaveActiveProject();


        std::string ToProjectRelativePath(const std::string& absolutePath);

    private:
        std::optional<Project> m_project;

        // internal helpers
        std::filesystem::path GetProjectFilePath(const std::filesystem::path& root) const;
        void EnsureProjectFolders(const std::filesystem::path& root) const;

        // UserScripts generation (NO VS template copy)
        bool GenerateUserScriptsProject(const std::filesystem::path& projectRootDir) const;

        void EnsureUserScriptsFolders(const std::filesystem::path& projectRootDir) const;
        bool GenerateUserScriptsVcxproj(const std::filesystem::path& projectRootDir) const;
        bool GenerateUserScriptsFilters(const std::filesystem::path& projectRootDir) const; // 선택(있으면 VS에서 보기 좋음)
        void GenerateDefaultScriptIfEmpty(const std::filesystem::path& projectRootDir) const;

    };
}
