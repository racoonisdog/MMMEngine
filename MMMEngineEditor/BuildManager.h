#pragma once
#include "Singleton.hpp"
#include <filesystem>
#include <string>
#include <functional>

namespace MMMEngine::Editor
{
    enum class BuildConfiguration
    {
        Debug,
        Release
    };

    enum class BuildResult
    {
        Success,
        Failed,
        NotFound,      // 프로젝트 파일 없음
        MSBuildNotFound // MSBuild.exe를 찾을 수 없음
    };

    struct BuildOutput
    {
        BuildResult result = BuildResult::Failed;
        std::string output;     // 빌드 출력 로그
        std::string errorLog;   // 에러 로그
        int exitCode = -1;
    };

    class BuildManager : public Utility::Singleton<BuildManager>
    {
    public:
        // 파일 변경 감지
        std::unordered_map<std::string, std::filesystem::file_time_type> m_fileTimestamps;

        bool HasFilesChanged(const std::filesystem::path& scriptsPath);

        // UserScripts 프로젝트 빌드
        BuildOutput BuildUserScripts(
            const std::filesystem::path& projectRootDir,
            BuildConfiguration config = BuildConfiguration::Debug
        );

        // 플레이어 빌드 (클린 빌드)
        // projectRootDir: 프로젝트 루트 절대 경로
        // outputDir: 출력 위치 절대 경로
        // config: 빌드 구성 (Debug/Release)
        BuildOutput BuildPlayer(
            const std::filesystem::path& projectRootDir,
            const std::filesystem::path& outputDir,
            BuildConfiguration config = BuildConfiguration::Release,
            const std::string & executableName = "MMMEnginePlayer"
        );

        // 빌드 진행 콜백 설정 (UI 업데이트용)
        using ProgressCallbackString = std::function<void(const std::string& message)>;
        using ProgressCallbackPercent = std::function<void(const float& message)>;
        void SetProgressCallbackString(ProgressCallbackString callback);
        void SetProgressCallbackPercent(ProgressCallbackPercent callback);

        /// Visual Studio devenv.exe 경로 (파일 열기 등에 사용). 없으면 빈 경로.
        std::filesystem::path FindDevEnv() const;

    private:
        // MSBuild.exe 경로 찾기
        std::filesystem::path FindMSBuild() const;

        // 빌드 명령 실행 (공통)
        BuildOutput ExecuteBuild(
            const std::filesystem::path& msbuildPath,
            const std::filesystem::path& vcxprojPath,
            BuildConfiguration config
        ) const;

        BuildOutput ExecuteBuildSolution(
            const std::filesystem::path& msbuildPath,
            const std::filesystem::path& slnPath,
            const std::string& projectName,
            BuildConfiguration config
        ) const;

        ProgressCallbackString m_progressCallbackString;
        ProgressCallbackPercent m_progressCallbackPercent;
    };
}
