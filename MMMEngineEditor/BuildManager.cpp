#include "BuildManager.h"
#include <Windows.h>
#include <array>
#include <sstream>

namespace fs = std::filesystem;

namespace MMMEngine::Editor
{
    void BuildManager::SetProgressCallbackString(ProgressCallbackString callback)
    {
        m_progressCallbackString = callback;
    }

    void BuildManager::SetProgressCallbackPercent(ProgressCallbackPercent callback)
    {
        m_progressCallbackPercent = callback;
    }

    fs::path BuildManager::FindMSBuild() const
    {
        // VS2022 기본 경로들
        std::array<const char*, 4> candidates = {
            R"(C:\Program Files\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe)",
            R"(C:\Program Files\Microsoft Visual Studio\2022\Professional\MSBuild\Current\Bin\MSBuild.exe)",
            R"(C:\Program Files\Microsoft Visual Studio\2022\Enterprise\MSBuild\Current\Bin\MSBuild.exe)",
            R"(C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\MSBuild\Current\Bin\MSBuild.exe)"
        };

        for (const auto& path : candidates)
        {
            if (fs::exists(path))
                return fs::path(path);
        }

        // vswhere.exe로 찾기 (더 정확함)
        const char* vswhere = R"(C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe)";
        if (fs::exists(vswhere))
        {
            // vswhere를 이용해 MSBuild 경로 찾기
            std::string cmd = std::string(vswhere) +
                R"( -latest -requires Microsoft.Component.MSBuild -find MSBuild\**\Bin\MSBuild.exe)";

            // 간단한 구현: popen으로 실행
            FILE* pipe = _popen(cmd.c_str(), "r");
            if (pipe)
            {
                char buffer[512];
                if (fgets(buffer, sizeof(buffer), pipe))
                {
                    std::string result = buffer;
                    // 개행 제거
                    if (!result.empty() && result.back() == '\n')
                        result.pop_back();
                    if (!result.empty() && result.back() == '\r')
                        result.pop_back();

                    _pclose(pipe);

                    if (fs::exists(result))
                        return fs::path(result);
                }
                _pclose(pipe);
            }
        }

        return {};
    }

    BuildOutput BuildManager::ExecuteBuild(
        const fs::path& msbuildPath,
        const fs::path& vcxprojPath,
        BuildConfiguration config
    ) const
    {
        BuildOutput output;

        const char* configStr = (config == BuildConfiguration::Debug) ? "Debug" : "Release";

        // MSBuild 명령 구성
        std::ostringstream cmdStream;
        cmdStream << "\"" << msbuildPath.string() << "\" "
            << "\"" << vcxprojPath.string() << "\" "
            << "/p:Configuration=" << configStr << " "
            << "/p:Platform=x64 "
            << "/v:minimal "  // 최소 출력
            << "/nologo";     // 로고 숨김

        std::string cmd = cmdStream.str();

        if (m_progressCallbackString)
            m_progressCallbackString(u8"빌드 시작: " + std::string(configStr));

        // CreateProcess로 실행하고 출력 캡처
        SECURITY_ATTRIBUTES sa;
        sa.nLength = sizeof(SECURITY_ATTRIBUTES);
        sa.bInheritHandle = TRUE;
        sa.lpSecurityDescriptor = NULL;

        HANDLE hReadPipe, hWritePipe;
        if (!CreatePipe(&hReadPipe, &hWritePipe, &sa, 0))
        {
            output.result = BuildResult::Failed;
            output.errorLog = "Failed to create pipe";
            return output;
        }

        SetHandleInformation(hReadPipe, HANDLE_FLAG_INHERIT, 0);

        STARTUPINFOA si = {};
        si.cb = sizeof(si);
        si.dwFlags = STARTF_USESTDHANDLES;
        si.hStdOutput = hWritePipe;
        si.hStdError = hWritePipe;

        PROCESS_INFORMATION pi = {};

        BOOL success = CreateProcessA(
            NULL,
            const_cast<char*>(cmd.c_str()),
            NULL, NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL, NULL,
            &si, &pi
        );

        CloseHandle(hWritePipe);

        if (!success)
        {
            CloseHandle(hReadPipe);
            output.result = BuildResult::Failed;
            output.errorLog = "Failed to create process";
            return output;
        }

        // 출력 읽기
        std::ostringstream outputStream;
        char buffer[4096];
        DWORD bytesRead;

        fs::path projectDir = vcxprojPath.parent_path();
        fs::path scriptsPath = projectDir / "Scripts";

        int totalFiles = 0;
        int currentFileCount = 0;

        if (fs::exists(scriptsPath) && fs::is_directory(scriptsPath))
        {
            for (const auto& entry : fs::recursive_directory_iterator(scriptsPath))
            {
                // 디렉토리가 아닌 실제 파일이며, 확장자가 .cpp인 것만 카운트
                if (fs::is_regular_file(entry) && entry.path().extension() == ".cpp")
                {
                    totalFiles++;
                }
            }
        }

        if (totalFiles == 0) totalFiles = 1;

        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
        {
            currentFileCount++;
            float percent = (static_cast<float>(currentFileCount) / totalFiles) * 100.0f;

            buffer[bytesRead] = '\0';
            std::string chunk(buffer);
            outputStream << buffer;

            if (chunk.find(".cpp") != std::string::npos)
            {
                currentFileCount++;
                float percent = (static_cast<float>(currentFileCount) / totalFiles) * 100.0f;
                if (percent > 100.0f) percent = 100.0f; // 100% 캡핑

                if (m_progressCallbackPercent)
                    m_progressCallbackPercent(percent);
            }

            if (m_progressCallbackString)
                m_progressCallbackString(std::string(buffer, bytesRead));
        }

        WaitForSingleObject(pi.hProcess, INFINITE);

        DWORD exitCode;
        GetExitCodeProcess(pi.hProcess, &exitCode);

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        CloseHandle(hReadPipe);

        output.output = outputStream.str();
        output.exitCode = static_cast<int>(exitCode);
        output.result = (exitCode == 0) ? BuildResult::Success : BuildResult::Failed;

        if (output.result == BuildResult::Failed)
            output.errorLog = output.output;

        if (m_progressCallbackString)
        {
            if (output.result == BuildResult::Success)
                m_progressCallbackString(u8"빌드 성공!");
            else
                m_progressCallbackString(u8"빌드 실패 (Exit Code: " + std::to_string(exitCode) + ")");
        }

        return output;
    }

    BuildOutput BuildManager::BuildUserScripts(
        const fs::path& projectRootDir,
        BuildConfiguration config
    )
    {
        BuildOutput output;

        // 1. vcxproj 파일 경로
        fs::path vcxprojPath = projectRootDir / "Source" / "UserScripts" / "UserScripts.vcxproj";
        if (!fs::exists(vcxprojPath))
        {
            output.result = BuildResult::NotFound;
            output.errorLog = "UserScripts.vcxproj not found at: " + vcxprojPath.string();
            return output;
        }

        // 2. MSBuild 찾기
        fs::path msbuildPath = FindMSBuild();
        if (msbuildPath.empty())
        {
            output.result = BuildResult::MSBuildNotFound;
            output.errorLog = "MSBuild.exe not found. Please install Visual Studio 2019 or later.";
            return output;
        }

        if (m_progressCallbackString)
            m_progressCallbackString(u8"MSBuild 경로: " + msbuildPath.string());

        // 3. 빌드 실행
        return ExecuteBuild(msbuildPath, vcxprojPath, config);
    }

    //BuildOutput BuildManager::BuildEngine(BuildConfiguration config)
    //{
    //    BuildOutput output;

    //    // TODO: 엔진 전체 빌드 구현
    //    // 예: MMMENGINE_DIR 환경변수에서 엔진 솔루션 찾기
    //    const char* engineDir = std::getenv("MMMENGINE_DIR");
    //    if (!engineDir)
    //    {
    //        output.result = BuildResult::NotFound;
    //        output.errorLog = "MMMENGINE_DIR environment variable not set";
    //        return output;
    //    }

    //    fs::path engineSln = fs::path(engineDir) / "MMMEngine.sln";
    //    if (!fs::exists(engineSln))
    //    {
    //        output.result = BuildResult::NotFound;
    //        output.errorLog = "Engine solution not found at: " + engineSln.string();
    //        return output;
    //    }

    //    fs::path msbuildPath = FindMSBuild();
    //    if (msbuildPath.empty())
    //    {
    //        output.result = BuildResult::MSBuildNotFound;
    //        output.errorLog = "MSBuild.exe not found";
    //        return output;
    //    }

    //    // .sln 파일도 ExecuteBuild와 유사하게 빌드 가능
    //    // 필요시 별도 함수로 분리
    //    output.result = BuildResult::Failed;
    //    output.errorLog = "Engine build not yet implemented";
    //    return output;
    //}

    //BuildOutput BuildManager::BuildGame(
    //    const fs::path& projectRootDir,
    //    BuildConfiguration config
    //)
    //{
    //    BuildOutput output;

    //    // TODO: 게임 실행파일 빌드 구현
    //    // UserScripts + 엔진 결합하여 최종 .exe 생성
    //    output.result = BuildResult::Failed;
    //    output.errorLog = "Game build not yet implemented";
    //    return output;
    //}
}