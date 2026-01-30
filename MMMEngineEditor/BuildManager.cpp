#include "BuildManager.h"
#include "UserScriptsGenerator.h"
#include <Windows.h>
#include <array>
#include <thread>
#include <sstream>

namespace fs = std::filesystem;

namespace MMMEngine::Editor
{
    BuildOutput BuildManager::ExecuteBuildSolution(
        const fs::path& msbuildPath,
        const fs::path& slnPath,
        const std::string& projectName,
        BuildConfiguration config
    ) const
    {
        BuildOutput output;

        const char* configStr = (config == BuildConfiguration::Debug) ? "Debug" : "Release";

        // MMMENGINE_DIR 환경변수 가져오기
        char* engineDirEnv = nullptr;
        size_t envSize = 0;
        _dupenv_s(&engineDirEnv, &envSize, "MMMENGINE_DIR");

        std::string engineDirStr;
        if (engineDirEnv != nullptr)
        {
            engineDirStr = engineDirEnv;
            free(engineDirEnv);
        }

        // MSBuild 명령 구성 (솔루션 빌드)
        std::ostringstream cmdStream;
        cmdStream << "\"" << msbuildPath.string() << "\" "
            << "\"" << slnPath.string() << "\" "
            << "/t:" << projectName << " "  // 특정 프로젝트만 빌드
            << "/p:Configuration=" << configStr << " "
            << "/p:Platform=x64 ";

        // MMMENGINE_DIR을 MSBuild 프로퍼티로 명시적으로 전달
        if (!engineDirStr.empty())
        {
            cmdStream << "/p:MMMENGINE_DIR=\"" << engineDirStr << "\" ";
        }

        cmdStream << "/p:DebugType=none "
            << "/v:minimal "  // 최소 출력
            << "/nologo";     // 로고 숨김

        std::string cmd = cmdStream.str();

        if (m_progressCallbackString)
            m_progressCallbackString(u8"솔루션 빌드 시작: " + projectName + " (" + std::string(configStr) + ")");

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

        // 환경변수 상속
        BOOL success = CreateProcessA(
            NULL,
            const_cast<char*>(cmd.c_str()),
            NULL, NULL,
            TRUE,
            CREATE_NO_WINDOW,
            NULL,  // 부모 프로세스의 환경변수 상속
            NULL,
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

        while (ReadFile(hReadPipe, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0)
        {
            buffer[bytesRead] = '\0';
            outputStream << buffer;

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
                m_progressCallbackString(u8"솔루션 빌드 성공: " + projectName);
            else
                m_progressCallbackString(u8"솔루션 빌드 실패: " + projectName + " (Exit Code: " + std::to_string(exitCode) + ")");
        }

        return output;
    }

    BuildOutput BuildManager::BuildPlayer(
        const fs::path& projectRootDir,
        const fs::path& outputDir,
        BuildConfiguration config,
        const std::string& executableName
    ) 
    {
        BuildOutput output;
        std::error_code ec;

        try
        {
            const char* configStr = (config == BuildConfiguration::Debug) ? "Debug" : "Release";

            // 1. 출력 디렉토리 생성
            if (m_progressCallbackString)
                m_progressCallbackString(u8"출력 디렉토리 생성 중...");

            if (!fs::exists(outputDir))
            {
                fs::create_directories(outputDir, ec);
                if (ec)
                {
                    output.result = BuildResult::Failed;
                    output.errorLog = "Failed to create output directory: " + ec.message();
                    return output;
                }
            }

            fs::path dataDir = outputDir / "Data";
            if (!fs::exists(dataDir))
            {
                fs::create_directories(dataDir, ec);
                if (ec)
                {
                    output.result = BuildResult::Failed;
                    output.errorLog = "Failed to create Data directory: " + ec.message();
                    return output;
                }
            }

            // 2. Assets 복사 (프로젝트 루트/Assets -> 출력위치/Data/Assets)
            if (m_progressCallbackString)
                m_progressCallbackString(u8"Assets 복사 중...");

            fs::path assetsSource = projectRootDir / "Assets";
            fs::path assetsDest = dataDir / "Assets";

            if (fs::exists(assetsSource))
            {
                fs::copy(assetsSource, assetsDest,
                    fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    output.result = BuildResult::Failed;
                    output.errorLog = "Failed to copy Assets: " + ec.message();
                    return output;
                }
            }

            // 3. ProjectSettings 복사 (project.json 제외)
            if (m_progressCallbackString)
                m_progressCallbackString(u8"ProjectSettings 복사 중...");

            fs::path settingsSource = projectRootDir / "ProjectSettings";
            fs::path settingsDest = dataDir / "Settings";

            if (fs::exists(settingsSource))
            {
                if (!fs::exists(settingsDest))
                {
                    fs::create_directories(settingsDest, ec);
                    if (ec)
                    {
                        output.result = BuildResult::Failed;
                        output.errorLog = "Failed to create Settings directory: " + ec.message();
                        return output;
                    }
                }

                // project.json을 제외하고 복사
                for (const auto& entry : fs::directory_iterator(settingsSource))
                {
                    if (entry.path().filename() != "project.json")
                    {
                        fs::path destPath = settingsDest / entry.path().filename();
                        fs::copy(entry.path(), destPath,
                            fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
                        if (ec)
                        {
                            output.result = BuildResult::Failed;
                            output.errorLog = "Failed to copy settings file: " + ec.message();
                            return output;
                        }
                    }
                }
            }

            // 4. Shader 복사
            if (m_progressCallbackString)
                m_progressCallbackString(u8"Shader 복사 중...");

            fs::path shaderSource = projectRootDir / "Shader";
            fs::path shaderDest = dataDir / "Shader";

            if (fs::exists(shaderSource))
            {
                fs::copy(shaderSource, shaderDest,
                    fs::copy_options::recursive | fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    output.result = BuildResult::Failed;
                    output.errorLog = "Failed to copy Shader: " + ec.message();
                    return output;
                }
            }

            // 5. UserScripts.dll 복사
            if (m_progressCallbackString)
                m_progressCallbackString(u8"UserScripts.dll 복사 중...");

            fs::path userScriptsDllSource = projectRootDir / "Binaries" / "Win64" / "UserScripts.dll";
            fs::path userScriptsDllDest = outputDir / "UserScripts.dll";

            if (fs::exists(userScriptsDllSource))
            {
                fs::copy_file(userScriptsDllSource, userScriptsDllDest,
                    fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    output.result = BuildResult::Failed;
                    output.errorLog = "Failed to copy UserScripts.dll: " + ec.message();
                    return output;
                }
            }
            else
            {
                output.result = BuildResult::NotFound;
                output.errorLog = "UserScripts.dll not found at: " + userScriptsDllSource.string();
                return output;
            }

            // 6. MMMENGINE_DIR 환경변수에서 DLL 복사
            if (m_progressCallbackString)
                m_progressCallbackString(u8"엔진 DLL 복사 중...");

            char* engineDirEnv = nullptr;
            size_t envSize = 0;
            _dupenv_s(&engineDirEnv, &envSize, "MMMENGINE_DIR");

            if (engineDirEnv == nullptr)
            {
                output.result = BuildResult::Failed;
                output.errorLog = "MMMENGINE_DIR environment variable not found";
                return output;
            }

            fs::path engineDir(engineDirEnv);
            free(engineDirEnv);

            fs::path engineBinDir = engineDir / "Common" / "Bin" / configStr;

            if (!fs::exists(engineBinDir))
            {
                output.result = BuildResult::NotFound;
                output.errorLog = "Engine bin directory not found: " + engineBinDir.string();
                return output;
            }

            // DLL 파일들 복사
            for (const auto& entry : fs::directory_iterator(engineBinDir))
            {
                if (entry.path().extension() == ".dll")
                {
                    fs::path destPath = outputDir / entry.path().filename();
                    fs::copy_file(entry.path(), destPath,
                        fs::copy_options::overwrite_existing, ec);
                    if (ec)
                    {
                        output.result = BuildResult::Failed;
                        output.errorLog = "Failed to copy DLL: " + ec.message();
                        return output;
                    }
                }
            }

            // 7. MMMEnginePlayer 프로젝트 빌드
            if (m_progressCallbackString)
                m_progressCallbackString(u8"MMMEnginePlayer 빌드 중...");

            // MSBuild 찾기
            fs::path msbuildPath = FindMSBuild();
            if (msbuildPath.empty())
            {
                output.result = BuildResult::MSBuildNotFound;
                output.errorLog = "MSBuild.exe not found";
                return output;
            }

            // 솔루션 파일 경로 찾기 (프로젝트 파일이 아닌 솔루션 파일 사용)
            fs::path slnPath = engineDir / "MMMEngine.sln";

            if (!fs::exists(slnPath))
            {
                output.result = BuildResult::NotFound;
                output.errorLog = "MMMEngine.sln not found at: " + slnPath.string();
                return output;
            }

            // 솔루션에서 MMMEnginePlayer 프로젝트만 빌드
            // /t:MMMEnginePlayer 옵션으로 특정 프로젝트만 빌드하면 의존성도 자동으로 빌드됨
            if (m_progressCallbackString)
                m_progressCallbackString(u8"솔루션에서 MMMEnginePlayer 프로젝트 빌드 시작...");

            BuildOutput playerBuildOutput = ExecuteBuildSolution(msbuildPath, slnPath, "MMMEnginePlayer", config);

            if (playerBuildOutput.result != BuildResult::Success)
            {
                output.result = playerBuildOutput.result;
                output.errorLog = "Failed to build MMMEnginePlayer: " + playerBuildOutput.errorLog;
                output.output = playerBuildOutput.output;
                return output;
            }

            // 빌드된 MMMEnginePlayer.exe 복사 (사용자 지정 이름으로)
            if (m_progressCallbackString)
                m_progressCallbackString(u8"실행 파일 복사 중...");

            fs::path playerExeSource = engineDir / "x64" / configStr / "MMMEnginePlayer.exe";

            // 사용자가 지정한 이름 사용 (확장자 추가)
            std::string finalExeName = executableName.empty() ? "MMMEnginePlayer.exe" : executableName + ".exe";
            fs::path playerExeDest = outputDir / finalExeName;

            if (!fs::exists(playerExeSource))
            {
                output.result = BuildResult::NotFound;
                output.errorLog = "MMMEnginePlayer.exe not found at: " + playerExeSource.string();
                return output;
            }

            fs::copy_file(playerExeSource, playerExeDest,
                fs::copy_options::overwrite_existing, ec);
            if (ec)
            {
                output.result = BuildResult::Failed;
                output.errorLog = "Failed to copy executable: " + ec.message();
                return output;
            }

            // MMMEngineShared.dll도 함께 복사
            if (m_progressCallbackString)
                m_progressCallbackString(u8"MMMEngineShared.dll 복사 중...");

            fs::path sharedDllSource = engineDir / "x64" / configStr / "MMMEngineShared.dll";
            fs::path sharedDllDest = outputDir / "MMMEngineShared.dll";

            if (fs::exists(sharedDllSource))
            {
                fs::copy_file(sharedDllSource, sharedDllDest,
                    fs::copy_options::overwrite_existing, ec);
                if (ec)
                {
                    output.result = BuildResult::Failed;
                    output.errorLog = "Failed to copy MMMEngineShared.dll: " + ec.message();
                    return output;
                }
            }
            else
            {
                // 경고는 하지만 빌드는 계속 진행 (선택적 DLL일 수 있음)
                if (m_progressCallbackString)
                    m_progressCallbackString(u8"[경고] MMMEngineShared.dll을 찾을 수 없습니다: " + sharedDllSource.string());
            }

            // 완료
            output.result = BuildResult::Success;
            output.output = "Player build completed successfully at: " + outputDir.string();

            if (m_progressCallbackString)
                m_progressCallbackString(u8"플레이어 빌드 완료!");

            if (m_progressCallbackPercent)
                m_progressCallbackPercent(100.0f);
        }
        catch (const fs::filesystem_error& e)
        {
            output.result = BuildResult::Failed;
            output.errorLog = "Filesystem error: " + std::string(e.what());
        }
        catch (const std::exception& e)
        {
            output.result = BuildResult::Failed;
            output.errorLog = "Error: " + std::string(e.what());
        }

        return output;
    }

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

    fs::path BuildManager::FindDevEnv() const
    {
        fs::path msbuild = FindMSBuild();
        if (!msbuild.empty())
        {
            // MSBuild: ...\MSBuild\Current\Bin\MSBuild.exe -> ...\Common7\IDE\devenv.exe
            fs::path vsRoot = msbuild.parent_path().parent_path().parent_path().parent_path();
            fs::path devenv = vsRoot / "Common7" / "IDE" / "devenv.exe";
            if (fs::exists(devenv))
                return devenv;
        }

        const char* vswhere = R"(C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe)";
        if (fs::exists(vswhere))
        {
            std::string cmd = std::string(vswhere) +
                R"( -latest -requires Microsoft.Component.MSBuild -find Common7\IDE\devenv.exe)";
            FILE* pipe = _popen(cmd.c_str(), "r");
            if (pipe)
            {
                char buffer[512];
                if (fgets(buffer, sizeof(buffer), pipe))
                {
                    std::string result = buffer;
                    if (!result.empty() && result.back() == '\n') result.pop_back();
                    if (!result.empty() && result.back() == '\r') result.pop_back();
                    _pclose(pipe);
                    if (fs::exists(result))
                        return fs::path(result);
                }
                _pclose(pipe);
            }
        }

        std::array<const char*, 3> candidates = {
            R"(C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\IDE\devenv.exe)",
            R"(C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\IDE\devenv.exe)",
            R"(C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\IDE\devenv.exe)"
        };
        for (const auto& path : candidates)
        {
            if (fs::exists(path))
                return fs::path(path);
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
            << "/p:DebugType=none "
            //<< "/m:" << std::thread::hardware_concurrency() << " "  // 병렬 빌드 (CPU 코어 수만큼)
            //<< "/p:CL_MPCount=" << std::thread::hardware_concurrency() << " "  // 컴파일러 병렬화
            //<< "/p:UseMultiToolTask=true "
            //<< "/p:EnforceProcessCountAcrossBuilds=true "
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

    bool BuildManager::HasFilesChanged(const fs::path& scriptsPath) {
        bool changed = false;

        for (const auto& entry : fs::recursive_directory_iterator(scriptsPath)) {
            if (entry.path().extension() == ".cpp" || entry.path().extension() == ".h") {
                auto lastWriteTime = fs::last_write_time(entry.path());
                auto& cachedTime = m_fileTimestamps[entry.path().string()];

                if (cachedTime != lastWriteTime) {
                    changed = true;
                    cachedTime = lastWriteTime;
                }
            }
        }

        return changed;
    }

    BuildOutput BuildManager::BuildUserScripts(
        const fs::path& projectRootDir,
        BuildConfiguration config
    )
    {
        BuildOutput output;

        // 0. 유저 스크립트 헤더 분석 + gen.cpp / 생성자 주입
        try
        {
            if (!UserScriptsGenerator::Generate(projectRootDir) && m_progressCallbackString)
                m_progressCallbackString(u8"[UserScriptsGenerator] 생성 실패 또는 스크립트 없음");
        }
        catch (const std::exception& e)
        {
            if (m_progressCallbackString)
                m_progressCallbackString(std::string(u8"[UserScriptsGenerator] ") + e.what());
        }

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
