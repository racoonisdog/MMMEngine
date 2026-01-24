#define NOMINMAX
#include <iostream>
#include <filesystem>

#include "GlobalRegistry.h"
#include "EditorRegistry.h"
#include "App.h"

#include "InputManager.h"
#include "ResourceManager.h"
#include "TimeManager.h"
#include "RenderManager.h"
#include "BehaviourManager.h"
#include "SceneManager.h"
#include "ObjectManager.h"
#include "ProjectManager.h"
#include "PhysxManager.h"

#include "StringHelper.h"
#include "ImGuiEditorContext.h"
#include "BuildManager.h"
#include "DLLHotLoadHelper.h"
#include "PhysX.h"
#include "ShaderInfo.h"

namespace fs = std::filesystem;
using namespace MMMEngine;
using namespace MMMEngine::Utility;
using namespace MMMEngine::Editor;

void AfterProjectLoaded()
{
	// 유저 스크립트 불러오기
	fs::path cwd = fs::current_path();
	DLLHotLoadHelper::CleanupHotReloadCopies(cwd);

	fs::path projectPath = ProjectManager::Get().GetActiveProject().rootPath;

	fs::path dllPath = DLLHotLoadHelper::CopyDllForHotReload(projectPath / "Binaries" / "Win64" / "UserScripts.dll", cwd);

	BehaviourManager::Get().StartUp(dllPath.stem().u8string());

	// 리소스 매니저 부팅
	ResourceManager::Get().StartUp(projectPath.generic_wstring() + L"/");
}

void Initialize()
{
	SetConsoleOutputCP(CP_UTF8);
	auto app = GlobalRegistry::g_pApp;
	auto hwnd = app->GetWindowHandle();
	auto windowInfo = app->GetWindowInfo();

	RenderManager::Get().StartUp(hwnd, windowInfo.width, windowInfo.height);
	ShaderInfo::Get().StartUp();
	InputManager::Get().StartUp(hwnd);
	app->OnWindowSizeChanged.AddListener<InputManager, &InputManager::HandleWindowResize>(&InputManager::Get());
	
	TimeManager::Get().StartUp();

	// 이전에 켰던 프로젝트 우선 확인
	EditorRegistry::g_editor_project_loaded = ProjectManager::Get().Boot();
	if (EditorRegistry::g_editor_project_loaded)
	{
		// 존재하는 경우 씬을 처음으로 스타트
		auto currentProject = ProjectManager::Get().GetActiveProject();
		SceneManager::Get().StartUp(currentProject.ProjectRootFS().generic_wstring() + L"/Assets/Scenes", currentProject.lastSceneIndex, true);
		app->SetWindowTitle(L"MMMEditor [ " + Utility::StringHelper::StringToWString(currentProject.rootPath) + L" ]");
		ObjectManager::Get().StartUp();


		// 경로 부팅
		AfterProjectLoaded();
		
		BuildManager::Get().SetProgressCallbackString([](const std::string& progress) { std::cout << progress.c_str() << std::endl; });

	}


	app->OnWindowSizeChanged.AddListener<RenderManager, &RenderManager::ResizeSwapChainSize>(&RenderManager::Get());

	Microsoft::WRL::ComPtr<ID3D11Device> device = RenderManager::Get().GetDevice();
	Microsoft::WRL::ComPtr<ID3D11DeviceContext> context = RenderManager::Get().GetContext();

	ImGuiEditorContext::Get().Initialize(hwnd, device.Get(), context.Get());
	app->OnBeforeWindowMessage.AddListener<ImGuiEditorContext, &ImGuiEditorContext::HandleWindowMessage>(&ImGuiEditorContext::Get());

	MMMEngine::PhysicX::Get().Initialize();
	SceneManager::Get().onSceneInitBefore.AddListenerLambda([]() { 
		PhysxManager::Get().UnbindScene();
		MMMEngine::PhysxManager::Get().BindScene(SceneManager::Get().GetCurrentSceneRaw());
		});
}

void Update_ProjectNotLoaded()
{
	TimeManager::Get().BeginFrame();
	InputManager::Get().Update();

	RenderManager::Get().BeginFrame();
	RenderManager::Get().Render();
	ImGuiEditorContext::Get().BeginFrame();
	ImGuiEditorContext::Get().Render();
	ImGuiEditorContext::Get().EndFrame();
	RenderManager::Get().EndFrame();

	if (EditorRegistry::g_editor_project_loaded)
	{
		auto currentProject = ProjectManager::Get().GetActiveProject();
		SceneManager::Get().StartUp(currentProject.ProjectRootFS().generic_wstring() + L"/Assets/Scenes", 0, true);
		GlobalRegistry::g_pApp->SetWindowTitle(L"MMMEditor [ " + Utility::StringHelper::StringToWString(currentProject.rootPath) + L" ]");

		ObjectManager::Get().StartUp();


		// 경로 부팅
		AfterProjectLoaded();
	
		BuildManager::Get().SetProgressCallbackString([](const std::string& progress) { std::cout << progress << std::endl; });
		return;
	}
}

void Update()
{
	if (!EditorRegistry::g_editor_project_loaded)
	{
		Update_ProjectNotLoaded();
		return;
	}

	TimeManager::Get().BeginFrame();
	InputManager::Get().Update();

	float dt = TimeManager::Get().GetDeltaTime();
	if (SceneManager::Get().CheckSceneIsChanged())
	{
		ObjectManager::Get().UpdateInternalTimer(dt);
		BehaviourManager::Get().DisableBehaviours();
		ObjectManager::Get().ProcessPendingDestroy();
		BehaviourManager::Get().AllSortBehaviours();
		BehaviourManager::Get().AllBroadCastBehaviourMessage("OnSceneLoaded");
	}

	//if (EditorRegistry::g_editor_scene_playing)
	//{
	//	BehaviourManager::Get().InitializeBehaviours();
	//}
	BehaviourManager::Get().InitializeBehaviours();

	TimeManager::Get().ConsumeFixedSteps([&](float fixedDt)
		{
			/*if (!EditorRegistry::g_editor_scene_playing)
				return;*/

			MMMEngine::PhysxManager::Get().StepFixed(fixedDt);
			BehaviourManager::Get().BroadCastBehaviourMessage("FixedUpdate");
		});

	BehaviourManager::Get().BroadCastBehaviourMessage("Update");

	RenderManager::Get().BeginFrame();
	RenderManager::Get().Render();
	ImGuiEditorContext::Get().BeginFrame();
	ImGuiEditorContext::Get().Render();
	ImGuiEditorContext::Get().EndFrame();
	RenderManager::Get().EndFrame();

	ObjectManager::Get().UpdateInternalTimer(dt);
	BehaviourManager::Get().DisableBehaviours();
	ObjectManager::Get().ProcessPendingDestroy();
}

void Release()
{
	PhysxManager::Get().UnbindScene();
	GlobalRegistry::g_pApp = nullptr;
	ImGuiEditorContext::Get().Uninitialize();
	RenderManager::Get().ShutDown();
	TimeManager::Get().ShutDown();
	InputManager::Get().ShutDown();

	SceneManager::Get().ShutDown();
	ObjectManager::Get().ShutDown();
	BehaviourManager::Get().ShutDown();

	fs::path cwd = fs::current_path();
	DLLHotLoadHelper::CleanupHotReloadCopies(cwd);
}

int main()
{
	App app{ L"MMMEditor",1600,900 };
	GlobalRegistry::g_pApp = &app;

	app.OnInitialize.AddListener<&Initialize>();
	app.OnUpdate.AddListener<&Update>();
	app.OnRelease.AddListener<&Release>();
	app.Run();
}