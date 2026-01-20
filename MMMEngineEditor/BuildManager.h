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
        // UserScripts 프로젝트 빌드
        BuildOutput BuildUserScripts(
            const std::filesystem::path& projectRootDir,
            BuildConfiguration config = BuildConfiguration::Debug
        );

        //// 엔진 전체 빌드 (TODO)
        //BuildOutput BuildEngine(
        //    BuildConfiguration config = BuildConfiguration::Debug
        //);

        //// 게임 실행 파일 빌드 (TODO)
        //BuildOutput BuildGame(
        //    const std::filesystem::path& projectRootDir,
        //    BuildConfiguration config = BuildConfiguration::Debug
        //);

        // 빌드 진행 콜백 설정 (UI 업데이트용)
        using ProgressCallbackString = std::function<void(const std::string& message)>;
        using ProgressCallbackPercent = std::function<void(const float& message)>;
        void SetProgressCallbackString(ProgressCallbackString callback);
        void SetProgressCallbackPercent(ProgressCallbackPercent callback);

    private:
        // MSBuild.exe 경로 찾기
        std::filesystem::path FindMSBuild() const;

        // 빌드 명령 실행 (공통)
        BuildOutput ExecuteBuild(
            const std::filesystem::path& msbuildPath,
            const std::filesystem::path& vcxprojPath,
            BuildConfiguration config
        ) const;

        ProgressCallbackString m_progressCallbackString;
        ProgressCallbackPercent m_progressCallbackPercent;
    };
}