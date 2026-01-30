#pragma once

#include <filesystem>
#include <string>

namespace MMMEngine::Editor
{
    /// 유저 스크립트 헤더 분석 + gen.cpp / 생성자 주입 코드 생성.
    /// BuildUserScripts 직전에 호출하여 Scripts/**/*.h 를 스캔하고
    /// UserScripts.gen.cpp 생성 및 각 헤더의 REGISTER_BEHAVIOUR_MESSAGE 주입.
    class UserScriptsGenerator
    {
    public:
        /// projectRootDir = 프로젝트 루트 (예: ProjectManager::Get().GetActiveProject().rootPath)
        /// 실패 시 예외 또는 로그만 남기고 계속할 수 있음 (false 반환).
        static bool Generate(const std::filesystem::path& projectRootDir);
    };
}
