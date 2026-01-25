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
using namespace Microsoft::WRL;

void AfterProjectLoaded()
{
	auto currentProject = ProjectManager::Get().GetActiveProject();
	SceneManager::Get().StartUp(currentProject.ProjectRootFS().generic_wstring() + L"/Assets/Scenes", 0, true);
	GlobalRegistry::g_pApp->SetWindowTitle(L"MMMEditor [ " + Utility::StringHelper::StringToWString(currentProject.rootPath) + L" ]");
	ObjectManager::Get().StartUp();

	// 유저 스크립트 불러오기
	fs::path cwd = fs::current_path();
	DLLHotLoadHelper::CleanupHotReloadCopies(cwd);

	fs::path projectPath = ProjectManager::Get().GetActiveProject().rootPath;

	fs::path dllPath = DLLHotLoadHelper::CopyDllForHotReload(projectPath / "Binaries" / "Win64" / "UserScripts.dll", cwd);

	BehaviourManager::Get().StartUp(dllPath.stem().u8string());

	// 리소스 매니저 부팅
	ResourceManager::Get().StartUp(projectPath.generic_wstring() + L"/");

	BuildManager::Get().SetProgressCallbackString([](const std::string& progress) { std::cout << progress.c_str() << std::endl; });
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
		// 경로 부팅
		AfterProjectLoaded();
	}

	app->OnWindowSizeChanged.AddListener<RenderManager, &RenderManager::ResizeSwapChainSize>(&RenderManager::Get());

	ComPtr<ID3D11Device> device = RenderManager::Get().GetDevice();
	ComPtr<ID3D11DeviceContext> context = RenderManager::Get().GetContext();

	ImGuiEditorContext::Get().Initialize(hwnd, device.Get(), context.Get());
	app->OnBeforeWindowMessage.AddListener<ImGuiEditorContext, &ImGuiEditorContext::HandleWindowMessage>(&ImGuiEditorContext::Get());

	PhysicX::Get().Initialize();
	SceneManager::Get().onSceneInitBefore.AddListenerLambda([]() { 
		PhysxManager::Get().UnbindScene();
		PhysxManager::Get().BindScene(SceneManager::Get().GetCurrentSceneRaw());
		});
}

void Update_ProjectNotLoaded()
{
	TimeManager::Get().BeginFrame();
	TimeManager::Get().ResetFixedStepAccumed();
	InputManager::Get().Update();

	RenderManager::Get().BeginFrame();
	RenderManager::Get().Render();
	ImGuiEditorContext::Get().BeginFrame();
	ImGuiEditorContext::Get().Render();
	ImGuiEditorContext::Get().EndFrame();
	RenderManager::Get().EndFrame();

	if (EditorRegistry::g_editor_project_loaded)
	{
		// 경로 부팅
		AfterProjectLoaded();
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

	if (EditorRegistry::g_editor_scene_playing)
	{
		BehaviourManager::Get().InitializeBehaviours();
	}
	TimeManager::Get().ConsumeFixedSteps([&](float fixedDt)
		{
			if (!EditorRegistry::g_editor_scene_playing)
				return;

			BehaviourManager::Get().BroadCastBehaviourMessage("FixedUpdate");
			PhysxManager::Get().StepFixed(fixedDt);

			std::vector<std::tuple<ObjPtr<GameObject>, ObjPtr<GameObject>, P_EvenType>> vec;

			std::swap(vec, PhysxManager::Get().GetCallbackQue());

			for (auto& [A, B, Event] : vec)
			{
				switch (Event)
				{
				case P_EvenType::C_enter:
					BehaviourManager::Get().SpecificBroadCastBehaviourMessage(A, "OnCollisionEnter", B);
					BehaviourManager::Get().SpecificBroadCastBehaviourMessage(B, "OnCollisionEnter", A);
					break;
				case P_EvenType::C_stay:
					BehaviourManager::Get().SpecificBroadCastBehaviourMessage(A, "OnCollisionStay", B);
					BehaviourManager::Get().SpecificBroadCastBehaviourMessage(B, "OnCollisionStay", A);
					break;
				case P_EvenType::C_out:
					BehaviourManager::Get().SpecificBroadCastBehaviourMessage(A, "OnCollisionExit", B);
					BehaviourManager::Get().SpecificBroadCastBehaviourMessage(B, "OnCollisionExit", A);
					break;
				case P_EvenType::T_enter:
					BehaviourManager::Get().SpecificBroadCastBehaviourMessage(A, "OnTriggerEnter", B);
					BehaviourManager::Get().SpecificBroadCastBehaviourMessage(B, "OnTriggerEnter", A);
					break;															
				case P_EvenType::T_out:											
					BehaviourManager::Get().SpecificBroadCastBehaviourMessage(A, "OnTriggerEnter", B);
					BehaviourManager::Get().SpecificBroadCastBehaviourMessage(B, "OnTriggerEnter", A);
					break;
				}
			}
		});

	BehaviourManager::Get().BroadCastBehaviourMessage("Update");
	BehaviourManager::Get().BroadCastBehaviourMessage("LateUpdate");

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