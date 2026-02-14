// ProjectManager.cpp
#include "ProjectManager.h"

#include <fstream>
#include <filesystem>
#include <cstdlib>
#include <cassert>
#include <vector>
#include <string>
#include <algorithm>
#include <json/json.hpp>

// MUID로 vcxproj ProjectGuid 만들기
#include "MUID.h"

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace MMMEngine::Editor
{
    static bool CopyEngineShaderToProjectRoot(const fs::path& projectRootDir)
    {
        const char* engineDirEnv = std::getenv("MMMENGINE_DIR");
        if (!engineDirEnv || engineDirEnv[0] == '\0')
            return false;

        fs::path engineRoot = fs::path(engineDirEnv);

        // source: C:\MMMEngine\Common\Shader
        fs::path src = engineRoot / "Common" / "Shader";

        // destination: C:\Project\Shader
        fs::path dst = projectRootDir / "Shader";

        std::error_code ec;

        if (!fs::exists(src, ec) || !fs::is_directory(src, ec))
            return false;

        // 목적지 Shader 폴더 생성
        fs::create_directories(dst, ec);
        if (ec) return false;

        // Shader 내부 전체 복사
        fs::copy(
            src,
            dst,
            fs::copy_options::recursive |
            fs::copy_options::overwrite_existing,
            ec
        );

        return !ec;
    }

    // ------------------------------------------------------------
    // Editor config (last opened project)
    // ------------------------------------------------------------
    static fs::path GetEditorConfigPath()
    {
        const char* appdata = std::getenv("APPDATA");
        fs::path base = appdata ? fs::path(appdata) : fs::current_path();
        fs::path dir = base / "MMMEngine" / "Editor";
        fs::create_directories(dir);
        return dir / "editor.json";
    }

    static void SaveLastProjectFile(const fs::path& projectFile)
    {
        fs::path cfg = GetEditorConfigPath();
        json j;
        j["lastProjectFile"] = projectFile.generic_u8string(); // '/' + UTF-8
        std::ofstream out(cfg, std::ios::binary);
        if (out) out << j.dump(4);
    }

    // ------------------------------------------------------------
    // GUID (ProjectGuid) using MUID
    // ------------------------------------------------------------
    static std::string MakeDeterministicProjectGuid(const fs::path& projectRootDir)
    {
        // vcxproj <ProjectGuid>는 프로젝트 루트 기준으로 "결정적"이어야 좋음
        const std::string key = projectRootDir.generic_u8string();

        // MUID가 이름 기반 결정적 UUID를 지원한다고 가정 (이전 대화 기준)
        MMMEngine::Utility::MUID id = MMMEngine::Utility::MUID::FromName(key);

        // MSBuild는 {GUID} 형태 선호
        // MUID에 이런 함수명이 없으면 ToString()/ToUpperString()에 맞춰 수정하면 됨
        // (너가 "muid는 이거쓰면됨"이라 했으니 여기만 네 MUID API에 맞게 1줄 수정하면 끝)
        return "{" + id.ToUpperString() + "}";
    }

    static void CollectUserScriptFiles(
        const fs::path& projectRootDir,
        std::vector<std::string>& outCpp,
        std::vector<std::string>& outHeaders)
    {
        outCpp.clear();
        outHeaders.clear();

        const fs::path scriptsDir = projectRootDir / "Source" / "UserScripts" / "Scripts";
        if (!fs::exists(scriptsDir) || !fs::is_directory(scriptsDir))
            return;

        auto toLower = [](std::string s) {
            for (char& c : s)
                c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            return s;
        };

        std::error_code ec;
        for (const auto& e : fs::recursive_directory_iterator(scriptsDir, fs::directory_options::skip_permission_denied, ec))
        {
            if (ec) break;
            if (!e.is_regular_file())
                continue;

            std::string ext = toLower(e.path().extension().string());
            bool isCpp = (ext == ".cpp" || ext == ".c" || ext == ".cc" || ext == ".cxx");
            bool isHeader = (ext == ".h" || ext == ".hpp");
            if (!isCpp && !isHeader)
                continue;

            fs::path rel = fs::relative(e.path(), projectRootDir / "Source" / "UserScripts");
            std::string relStr = rel.generic_string();
            std::replace(relStr.begin(), relStr.end(), '/', '\\');

            if (isCpp)
                outCpp.push_back(relStr);
            else
                outHeaders.push_back(relStr);
        }

        std::sort(outCpp.begin(), outCpp.end());
        std::sort(outHeaders.begin(), outHeaders.end());
    }

    // ------------------------------------------------------------
    // ProjectManager basic
    // ------------------------------------------------------------
    bool ProjectManager::HasActiveProject() const
    {
        return m_project.has_value();
    }

    const Project& ProjectManager::GetActiveProject() const
    {
        assert(m_project.has_value());
        return *m_project;
    }

    void ProjectManager::SetLastSceneIndex(uint32_t sceneIndex)
    {
        if (m_project)
        {
            m_project->lastSceneIndex = sceneIndex;
		}
    }

    fs::path ProjectManager::GetProjectFilePath(const fs::path& root) const
    {
        return root / "ProjectSettings" / "project.json";
    }

    void ProjectManager::EnsureProjectFolders(const fs::path& root) const
    {
        fs::create_directories(root / "ProjectSettings");
        fs::create_directories(root / "Assets" / "Scenes");
        fs::create_directories(root / "Source");
        fs::create_directories(root / "Binaries" / "Win64");
        fs::create_directories(root / "Build");
    }

    std::optional<fs::path> ProjectManager::SaveActiveProject()
    {
        if (!m_project) return std::nullopt;

        fs::path root = fs::path(m_project->rootPath);
        EnsureProjectFolders(root);

        fs::path projFile = GetProjectFilePath(root);

        json j;
        j["rootPath"] = root.generic_u8string();
        j["lastSceneIndex"] = m_project->lastSceneIndex;

        std::ofstream out(projFile, std::ios::binary);
        if (!out) return std::nullopt;

        out << j.dump(4);
        return projFile;
    }

    bool ProjectManager::OpenProject(const std::filesystem::path& projectFile)
    {
        namespace fs = std::filesystem;

        if (!fs::exists(projectFile))
        {
            return false;
        }

        // JSON 파일 읽기
        std::ifstream ifs(projectFile);
        if (!ifs.is_open())
        {
            return false;
        }

        nlohmann::json j;
        try
        {
            ifs >> j;
        }
        catch (...)
        {
            return false;
        }
        ifs.close();

        // Project 객체 생성 및 JSON에서 데이터 읽기
        Project proj;

        // JSON에서 lastSceneIndex 읽기 (없으면 기본값 0)
        if (j.contains("lastSceneIndex"))
        {
            proj.lastSceneIndex = j["lastSceneIndex"].get<uint32_t>();
        }
        else
        {
            proj.lastSceneIndex = 0;
        }

        // rootPath를 현재 찾은 프로젝트 폴더로 업데이트
        // projectFile은 "프로젝트루트/ProjectSettings/project.json"
        fs::path actualRoot = projectFile.parent_path().parent_path();

        // "/" 구분자로 변환
        std::string rootPathStr = actualRoot.generic_string();
        proj.rootPath = rootPathStr;

        // 업데이트된 내용을 다시 저장
        try
        {
            std::ofstream ofs(projectFile);
            if (ofs.is_open())
            {
                nlohmann::json updatedJson;
                updatedJson["rootPath"] = proj.rootPath;
                updatedJson["lastSceneIndex"] = proj.lastSceneIndex;

                ofs << updatedJson.dump(4);
                ofs.close();
            }
        }
        catch (...)
        {
            // 저장 실패해도 프로젝트는 로드
        }

        m_project = proj;

        const fs::path projectRootDir = fs::path(m_project->rootPath);
        EnsureUserScriptsFolders(projectRootDir);

        const fs::path vcxprojPath = projectRootDir / "Source" / "UserScripts" / "UserScripts.vcxproj";
        const fs::path slnPath = projectRootDir / "Source" / "UserScripts" / "UserScripts.sln";
        if (!fs::exists(vcxprojPath))
        {
            GenerateUserScriptsProject(projectRootDir);
        }
        else
        {
            if (!fs::exists(slnPath))
            {
                GenerateUserScriptsSolution(projectRootDir);
            }
            GenerateVSCodeSettings(projectRootDir);
        }

        return true;
    }

    // ------------------------------------------------------------
    // UserScripts generation
    // ------------------------------------------------------------
    void ProjectManager::EnsureUserScriptsFolders(const fs::path& projectRootDir) const
    {
        fs::create_directories(projectRootDir / "Source" / "UserScripts" / "Scripts");
        fs::create_directories(projectRootDir / "Binaries" / "Win64");
        fs::create_directories(projectRootDir / "Build");
    }

    void ProjectManager::GenerateDefaultScriptIfEmpty(const fs::path& projectRootDir) const
    {
        fs::path scriptsDir = projectRootDir / "Source" / "UserScripts" / "Scripts";

        bool hasCpp = false;
        std::error_code ec;
        for (auto& e : fs::recursive_directory_iterator(scriptsDir, ec))
        {
            if (ec) break;
            if (!e.is_regular_file()) continue;
            auto ext = e.path().extension().wstring();
            if (ext == L".cpp" || ext == L".cc" || ext == L".cxx")
            {
                hasCpp = true;
                break;
            }
        }
        if (hasCpp) return;

        fs::path file = scriptsDir / "ExampleBehaviour.h";
        std::ofstream out(file, std::ios::binary);
        if (!out) return;

        // NOTE: gen.cpp가 RTTR 등록. USCRIPT_MESSAGE / USCRIPT_PROPERTY 로 생성기가 인식.
        out <<
			R"(#pragma once
#include "rttr/type"
#include "ScriptBehaviour.h"
#include "UserScriptsCommon.h"
#include <string>

namespace MMMEngine
{
    class USERSCRIPTS ExampleBehaviour : public ScriptBehaviour
    {
    private:
        RTTR_ENABLE(ScriptBehaviour)
        RTTR_REGISTRATION_FRIEND
    public:
        ExampleBehaviour()
        {
        }

        USCRIPT_PROPERTY()
        bool isCustomBool = true;
        USCRIPT_PROPERTY()
        float customFloat = 14.0f;
        USCRIPT_PROPERTY()
        int customInt = 2;
        USCRIPT_PROPERTY()
        std::string customString = "Hello, World!";

        USCRIPT_PROPERTY_HIDDEN()
        bool isDontShowBool = false;

        USCRIPT_MESSAGE()
        void Start();
        USCRIPT_MESSAGE()
        void Update();
    };
}
)";

        fs::path file2 = scriptsDir / "ExampleBehaviour.cpp";
        std::ofstream out2(file2, std::ios::binary);
        if (!out2) return;

        // NOTE: RTTR 등록은 gen.cpp에서 자동. .cpp에는 구현만 둠.
        out2 <<
            R"(#include "Export.h"
#include "ScriptBehaviour.h"
#include "ExampleBehaviour.h"

void MMMEngine::ExampleBehaviour::Start()
{
}

void MMMEngine::ExampleBehaviour::Update()
{
}
)";
    }

    std::string ProjectManager::ToProjectRelativePath(const std::string& pathStr)
    {
        if (!HasActiveProject())
            return pathStr;

        const auto& project = GetActiveProject();
        namespace fs = std::filesystem;

        fs::path root = fs::path(project.rootPath);              // e.g. C:\Users\...\murasaki
        fs::path in = fs::path(pathStr);

        // "absolutePath"라고 들어오지만 실제로는 상대경로일 수 있음
        fs::path absCandidate = in.is_absolute() ? in : (root / in);

        std::error_code ec;
        fs::path rootCan = fs::weakly_canonical(root, ec);
        if (ec) return pathStr;

        fs::path absCan = fs::weakly_canonical(absCandidate, ec);
        if (ec) return pathStr;

        // 프로젝트 내부인지 확인
        auto [rootEnd, absEnd] = std::mismatch(rootCan.begin(), rootCan.end(), absCan.begin());
        if (rootEnd == rootCan.end())
        {
            fs::path rel = absCan.lexically_relative(rootCan);
            std::string result = rel.generic_string(); // '/'로 통일
            return result;
        }

        return absCan.string(); // 외부면 정규화된 절대경로 반환 (원하면 원본 유지)
    }

    bool ProjectManager::GenerateUserScriptsVcxproj(const fs::path& projectRootDir) const
    {
        const fs::path projDir = projectRootDir / "Source" / "UserScripts";
        const fs::path vcxprojPath = projDir / "UserScripts.vcxproj";

        const std::string guid = MakeDeterministicProjectGuid(projectRootDir);

        // EngineShared 경로: MSBuild 매크로 사용 (MMMENGINE_DIR)
        // MMMENGINE_DIR가 없으면 상대경로로 fallback 하도록 .vcxproj에 기본값 지정
        std::string engineSharedInclude = R"($(MMMENGINE_DIR)\MMMEngineShared)";
        std::string engineSharedIncludeDXTk = R"($(MMMENGINE_DIR)\MMMEngineShared\dxtk)";
        std::string engineSharedIncludeDXTkInc = R"($(MMMENGINE_DIR)\MMMEngineShared\dxtk\inc)";
        std::string engineSharedIncludePhysXInc = R"($(MMMENGINE_DIR)\MMMEngineShared\physx)";
        std::string engineSharedDebugLibDir = R"($(MMMENGINE_DIR)\X64\Debug)";
        std::string engineSharedReleaseLibDir = R"($(MMMENGINE_DIR)\X64\Release)";
        std::string engineSharedCommonDebugLibDir = R"($(MMMENGINE_DIR)\Common\Lib\Debug)";
        std::string engineSharedCommonReleaseLibDir = R"($(MMMENGINE_DIR)\Common\Lib\Release)";
        std::string engineSharedLibName = "MMMEngineShared.lib";
        std::string DirectXLibName = "DirectXTK.lib;DirectXTex.lib";
        std::string rttrDebugLibName = "rttr_core_d.lib";
        std::string rttrReleaseLibName = "rttr_core.lib";
        std::string physXLibsName = "PhysXCommon_64.lib;PhysXCooking_64.lib;PhysXExtensions_static_64.lib;PhysXFoundation_64.lib";
        std::string renderResourceLibs = "assimp-vc143-mt.lib;pugixml.lib;minizip.lib;zlib.lib;kubazip.lib;poly2tri.lib;draco.lib";

        std::ofstream out(vcxprojPath, std::ios::binary);
        if (!out) return false;

        // UTF-8 BOM 추가
        const char bom[] = "\xEF\xBB\xBF";
        out.write(bom, 3);

        // VS2022(v143) 기준. 필요하면 v142로 변경.
        out <<
            R"xml(<?xml version="1.0" encoding="utf-8"?>
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">

  <ItemGroup Label="ProjectConfigurations">
    <ProjectConfiguration Include="Debug|x64">
      <Configuration>Debug</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
    <ProjectConfiguration Include="Release|x64">
      <Configuration>Release</Configuration>
      <Platform>x64</Platform>
    </ProjectConfiguration>
  </ItemGroup>

  <PropertyGroup Label="Globals">
    <ProjectGuid>)xml" << guid << R"xml(</ProjectGuid>
    <Keyword>Win32Proj</Keyword>
    <RootNamespace>UserScripts</RootNamespace>
    <WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion>
  </PropertyGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.Default.props" />

  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'" Label="Configuration">
    <ConfigurationType>DynamicLibrary</ConfigurationType>
    <UseDebugLibraries>true</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <CharacterSet>MultiByte</CharacterSet>
  </PropertyGroup>

  <PropertyGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'" Label="Configuration">
    <ConfigurationType>DynamicLibrary</ConfigurationType>
    <UseDebugLibraries>false</UseDebugLibraries>
    <PlatformToolset>v143</PlatformToolset>
    <WholeProgramOptimization>true</WholeProgramOptimization>
    <CharacterSet>MultiByte</CharacterSet>
  </PropertyGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.props" />
  <ImportGroup Label="ExtensionSettings" />
  <ImportGroup Label="Shared" />

  <ImportGroup Label="PropertySheets" Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props"
            Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')"
            Label="LocalAppDataPlatform" />
  </ImportGroup>
  
  <ImportGroup Label="PropertySheets" Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <Import Project="$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props"
            Condition="exists('$(UserRootDir)\Microsoft.Cpp.$(Platform).user.props')"
            Label="LocalAppDataPlatform" />
  </ImportGroup>

  <PropertyGroup Label="UserMacros" />

  <!-- MMMENGINE_DIR 기본값 (없으면 프로젝트 기준 상대경로) -->
  <PropertyGroup>
    <MMMENGINE_DIR Condition="'$(MMMENGINE_DIR)'==''">$(ProjectDir)..\..\..</MMMENGINE_DIR>
  </PropertyGroup>

  <!--출력 고정: ProjectRoot/Binaries/Win64/UserScripts.dll-->
  <PropertyGroup>
    <OutDir>$(ProjectDir)..\..\Binaries\Win64\</OutDir>
    <TargetName>UserScripts</TargetName>
    <IntDir>$(ProjectDir)..\..\Build\UserScripts\$(Configuration)\</IntDir>
  </PropertyGroup>

  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Debug|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <LanguageStandard>stdcpp17</LanguageStandard>
      <ConformanceMode>false</ConformanceMode>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
      <DisableSpecificWarnings>4819;4251;%(DisableSpecificWarnings)</DisableSpecificWarnings>
      <PreprocessorDefinitions>WIN32;_WINDOWS;_DEBUG;RTTR_DLL;USERSCRIPTS_EXPORT;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <AdditionalIncludeDirectories>)xml" << engineSharedInclude << R"xml(;)xml" << engineSharedIncludeDXTk << R"xml(;)xml" << engineSharedIncludeDXTkInc << R"xml(;)xml" << engineSharedIncludePhysXInc << R"xml(;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
      <!-- Debug 빌드에서는 항상 /bigobj를 적용하고, 기존 AdditionalOptions는 %(AdditionalOptions)로 보존 -->
      <AdditionalOptions>/bigobj %(AdditionalOptions)</AdditionalOptions>
    </ClCompile>
    <Link>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <ProgramDatabaseFile>$(IntDir)$(TargetName).pdb</ProgramDatabaseFile>
      <AdditionalLibraryDirectories>)xml" << engineSharedDebugLibDir << R"xml(;)xml" << engineSharedCommonDebugLibDir << R"xml(;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
      <AdditionalDependencies>)xml" << engineSharedLibName << R"xml(;)xml" << DirectXLibName << R"xml(;)xml" << rttrDebugLibName << R"xml(;)xml" << physXLibsName << R"xml(;)xml" << renderResourceLibs << R"xml(;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>

  <ItemDefinitionGroup Condition="'$(Configuration)|$(Platform)'=='Release|x64'">
    <ClCompile>
      <WarningLevel>Level3</WarningLevel>
      <LanguageStandard>stdcpp17</LanguageStandard>
      <ConformanceMode>false</ConformanceMode>
      <DebugInformationFormat>ProgramDatabase</DebugInformationFormat>
      <FunctionLevelLinking>true</FunctionLevelLinking>
      <IntrinsicFunctions>true</IntrinsicFunctions>
      <DisableSpecificWarnings>4819;4251;%(DisableSpecificWarnings)</DisableSpecificWarnings>
      <PreprocessorDefinitions>WIN32;_WINDOWS;NDEBUG;RTTR_DLL;USERSCRIPTS_EXPORT;%(PreprocessorDefinitions)</PreprocessorDefinitions>
      <AdditionalIncludeDirectories>)xml" << engineSharedInclude << R"xml(;)xml" << engineSharedIncludeDXTk << R"xml(;)xml" << engineSharedIncludeDXTkInc << R"xml(;)xml" << engineSharedIncludePhysXInc << R"xml(;%(AdditionalIncludeDirectories)</AdditionalIncludeDirectories>
    </ClCompile>
    <Link>
      <GenerateDebugInformation>true</GenerateDebugInformation>
      <ProgramDatabaseFile>$(IntDir)$(TargetName).pdb</ProgramDatabaseFile>
      <EnableCOMDATFolding>true</EnableCOMDATFolding>
      <OptimizeReferences>true</OptimizeReferences>
      <AdditionalLibraryDirectories>)xml" << engineSharedReleaseLibDir << R"xml(;)xml" << engineSharedCommonReleaseLibDir << R"xml(;%(AdditionalLibraryDirectories)</AdditionalLibraryDirectories>
      <AdditionalDependencies>)xml" << engineSharedLibName << R"xml(;)xml" << DirectXLibName << R"xml(;)xml" << rttrReleaseLibName << R"xml(;)xml" << physXLibsName << R"xml(;)xml" << renderResourceLibs << R"xml(;%(AdditionalDependencies)</AdditionalDependencies>
    </Link>
  </ItemDefinitionGroup>

  <ItemGroup>
)xml";

        std::vector<std::string> cppFiles;
        std::vector<std::string> headerFiles;
        CollectUserScriptFiles(projectRootDir, cppFiles, headerFiles);
        for (const auto& file : cppFiles)
            out << "    <ClCompile Include=\"" << file << "\" />\n";
        for (const auto& file : headerFiles)
            out << "    <ClInclude Include=\"" << file << "\" />\n";

        out <<
            R"xml(  </ItemGroup>

  <Import Project="$(VCTargetsPath)\Microsoft.Cpp.targets" />
  <ImportGroup Label="ExtensionTargets" />
</Project>
)xml";

            return true;
    }

    bool ProjectManager::GenerateUserScriptsFilters(const fs::path& projectRootDir) const
    {
        const fs::path projDir = projectRootDir / "Source" / "UserScripts";
        const fs::path filtersPath = projDir / "UserScripts.vcxproj.filters";

        std::ofstream out(filtersPath, std::ios::binary);
        if (!out) return false;

        // VS에서 Scripts 폴더로 깔끔하게 보이기 용도(빌드엔 없어도 됨)
        out <<
            R"(<?xml version="1.0" encoding="utf-8"?>
<Project ToolsVersion="4.0" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup>
    <Filter Include="Scripts">
      <UniqueIdentifier>{B0B9C8A7-2F1D-4C31-9A6C-0D0C0F0A0001}</UniqueIdentifier>
    </Filter>
  </ItemGroup>
  <ItemGroup>
)";

        std::vector<std::string> cppFiles;
        std::vector<std::string> headerFiles;
        CollectUserScriptFiles(projectRootDir, cppFiles, headerFiles);
        for (const auto& file : cppFiles)
            out << "    <ClCompile Include=\"" << file << "\"><Filter>Scripts</Filter></ClCompile>\n";
        for (const auto& file : headerFiles)
            out << "    <ClInclude Include=\"" << file << "\"><Filter>Scripts</Filter></ClInclude>\n";

        out <<
            R"(  </ItemGroup>
</Project>
)";
        return true;
    }

    bool ProjectManager::GenerateUserScriptsSolution(const fs::path& projectRootDir) const
    {
        const fs::path projDir = projectRootDir / "Source" / "UserScripts";
        const fs::path slnPath = projDir / "UserScripts.sln";

        const std::string projGuid = MakeDeterministicProjectGuid(projectRootDir);
        const char* vcxprojTypeGuid = "{8BC9CEB8-8B4A-11D0-8D11-00A0C91BC942}";

        std::ofstream out(slnPath, std::ios::binary);
        if (!out) return false;

        out <<
            "Microsoft Visual Studio Solution File, Format Version 12.00\n"
            "# Visual Studio Version 17\n"
            "VisualStudioVersion = 17.0.31903.59\n"
            "MinimumVisualStudioVersion = 10.0.40219.1\n"
            "Project(\"" << vcxprojTypeGuid << "\") = \"UserScripts\", \"UserScripts.vcxproj\", \"" << projGuid << "\"\n"
            "EndProject\n"
            "Global\n"
            "\tGlobalSection(SolutionConfigurationPlatforms) = preSolution\n"
            "\t\tDebug|x64 = Debug|x64\n"
            "\t\tRelease|x64 = Release|x64\n"
            "\tEndGlobalSection\n"
            "\tGlobalSection(ProjectConfigurationPlatforms) = postSolution\n"
            "\t\t" << projGuid << ".Debug|x64.ActiveCfg = Debug|x64\n"
            "\t\t" << projGuid << ".Debug|x64.Build.0 = Debug|x64\n"
            "\t\t" << projGuid << ".Release|x64.ActiveCfg = Release|x64\n"
            "\t\t" << projGuid << ".Release|x64.Build.0 = Release|x64\n"
            "\tEndGlobalSection\n"
            "EndGlobal\n";

        return true;
    }

    bool ProjectManager::GenerateVSCodeSettings(const fs::path& projectRootDir) const
    {
        const fs::path vscodeDir = projectRootDir / ".vscode";
        const fs::path propsPath = vscodeDir / "c_cpp_properties.json";

        std::error_code ec;
        fs::create_directories(vscodeDir, ec);
        if (ec) return false;

        json debugConfig;
        debugConfig["name"] = "Win64-Debug";
        debugConfig["includePath"] = json::array({
            "${workspaceFolder}/Source/UserScripts",
            "${workspaceFolder}/Source/UserScripts/Scripts",
            "${workspaceFolder}/Source",
            "${workspaceFolder}",
            "${env:MMMENGINE_DIR}/MMMEngineShared",
            "${env:MMMENGINE_DIR}/MMMEngineShared/dxtk",
            "${env:MMMENGINE_DIR}/MMMEngineShared/dxtk/inc",
            "${env:MMMENGINE_DIR}/MMMEngineShared/physx"
        });
        debugConfig["defines"] = json::array({
            "WIN32",
            "_WINDOWS",
            "_DEBUG",
            "RTTR_DLL",
            "USERSCRIPTS_EXPORT"
        });
        debugConfig["compilerPath"] = "cl.exe";
        debugConfig["cStandard"] = "c17";
        debugConfig["cppStandard"] = "c++17";
        debugConfig["intelliSenseMode"] = "windows-msvc-x64";

        json releaseConfig = debugConfig;
        releaseConfig["name"] = "Win64-Release";
        releaseConfig["defines"] = json::array({
            "WIN32",
            "_WINDOWS",
            "NDEBUG",
            "RTTR_DLL",
            "USERSCRIPTS_EXPORT"
        });

        json root;
        root["configurations"] = json::array({ debugConfig, releaseConfig });
        root["version"] = 4;

        std::ofstream out(propsPath, std::ios::binary);
        if (!out) return false;

        out << root.dump(4);
        return true;
    }

    bool ProjectManager::GenerateUserScriptsProject(const fs::path& projectRootDir) const
    {
        EnsureUserScriptsFolders(projectRootDir);

        if (!GenerateUserScriptsVcxproj(projectRootDir))
            return false;

        // filters는 실패해도 치명적이지 않음
        GenerateUserScriptsFilters(projectRootDir);
        GenerateUserScriptsSolution(projectRootDir);

        GenerateVSCodeSettings(projectRootDir);
        GenerateDefaultScriptIfEmpty(projectRootDir);
        return true;
    }

    // ------------------------------------------------------------
    // CreateNewProject / Boot
    // ------------------------------------------------------------
    bool ProjectManager::CreateNewProject(const fs::path& projectRootDir)
    {
        if (projectRootDir.empty()) return false;

        EnsureProjectFolders(projectRootDir);

        if (!CopyEngineShaderToProjectRoot(projectRootDir))
            return false; // 또는 로그만 남기고 계속

        Project p;
        p.rootPath = projectRootDir.generic_u8string();
        p.lastSceneIndex = 0;
        m_project = p;

        auto saved = SaveActiveProject();
        if (!saved) return false;

        // 템플릿 복사 제거 -> vcxproj 직접 생성
        if (!GenerateUserScriptsProject(projectRootDir)) return false;

        SaveLastProjectFile(*saved);
        return true;
    }

    bool ProjectManager::Boot()
    {
        // 최근 프로젝트 자동 오픈만 시도
        fs::path cfg = GetEditorConfigPath();
        if (!fs::exists(cfg))
            return false;

        json j;
        {
            std::ifstream in(cfg, std::ios::binary);
            if (!in) return false;
            try { in >> j; }
            catch (...) { return false; }
        }

        if (!j.contains("lastProjectFile") || !j["lastProjectFile"].is_string())
            return false;

        fs::path lastProj = fs::path(j["lastProjectFile"].get<std::string>());
        if (!fs::exists(lastProj))
            return false;

        return OpenProject(lastProj);
    }

    void ProjectManager::RefreshUserScriptsIDEFiles()
    {
        if (!HasActiveProject())
            return;

        const fs::path projectRootDir = fs::path(GetActiveProject().rootPath);
        EnsureUserScriptsFolders(projectRootDir);

        GenerateUserScriptsVcxproj(projectRootDir);
        GenerateUserScriptsFilters(projectRootDir);
        GenerateUserScriptsSolution(projectRootDir);
        GenerateVSCodeSettings(projectRootDir);
    }
}
