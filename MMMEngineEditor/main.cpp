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
#include "PhysicsSettings.h"

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


	PhysicsSettings::Get().StartUp(currentProject.ProjectRootFS() / "ProjectSettings");

	// 유저 스크립트 불러오기
	fs::path hotDir;
	if (const char* lad = std::getenv("LOCALAPPDATA"))
		hotDir = fs::path(lad) / "MMMEngine" / "HotReload";
	else
		hotDir = fs::temp_directory_path() / "MMMEngine" / "HotReload";

	DLLHotLoadHelper::CleanupHotReloadCopies(hotDir);

	fs::path projectPath = ProjectManager::Get().GetActiveProject().rootPath;
	fs::path originDll = projectPath / "Binaries" / "Win64" / "UserScripts.dll";

	fs::path dllPath = DLLHotLoadHelper::CopyDllForHotReload(originDll, hotDir);

	BehaviourManager::Get().StartUp(dllPath.u8string());

	// 리소스 매니저 부팅
	ResourceManager::Get().StartUp(projectPath.generic_wstring() + L"/");

	// 쉐이더 인포 시작하기
	ShaderInfo::Get().StartUp();
	
	BuildManager::Get().SetProgressCallbackString([](const std::string& progress) { std::cout << progress.c_str() << std::endl; });
}

void Initialize()
{
	SetConsoleOutputCP(CP_UTF8);
	auto app = GlobalRegistry::g_pApp;
	auto hwnd = app->GetWindowHandle();
	auto windowInfo = app->GetWindowInfo();

	RenderManager::Get().StartUp(hwnd, windowInfo.width, windowInfo.height);
	InputManager::Get().StartUp(hwnd);
	app->OnWindowSizeChanged.AddListener<InputManager, &InputManager::HandleWindowResize>(&InputManager::Get());
	app->OnMouseWheelUpdate.AddListener<InputManager, &InputManager::HandleMouseWheelEvent>(&InputManager::Get());
	
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

	if (EditorRegistry::g_editor_scene_playing
		&& !EditorRegistry::g_editor_scene_pause)
	{
		BehaviourManager::Get().InitializeBehaviours();
	}

	TimeManager::Get().ConsumeFixedSteps([&](float fixedDt)
		{
			if (!EditorRegistry::g_editor_scene_playing
				|| EditorRegistry::g_editor_scene_pause)
			{
				PhysxManager::Get().SetStep();
				return;
			}

			BehaviourManager::Get().BroadCastBehaviourMessage("FixedUpdate");
			PhysxManager::Get().StepFixed(fixedDt);

			std::vector<std::variant<CollisionInfo, TriggerInfo>> vec;

			std::swap(vec, PhysxManager::Get().GetCallbackQue());

			for (auto& ev : vec)
			{
				//기존은 enum으로 switch 분기, variant를 쓰면서 실제타입분리를 위해 visit를 사용
				//variant 안에 들어 있는 실제 타입에 따라 함수를 호출
				std::visit([&](auto& e)
					{
						using T = std::decay_t<decltype(e)>;

						if constexpr (std::is_same_v<T, CollisionInfo>)
						{
							switch (e.phase)
							{
							case CollisionPhase::Enter:
								BehaviourManager::Get().SpecificBroadCastBehaviourMessage(e.self, "OnCollisionEnter", e);
								break;
							case CollisionPhase::Stay:
								BehaviourManager::Get().SpecificBroadCastBehaviourMessage(e.self, "OnCollisionStay", e);
								break;
							case CollisionPhase::Exit:
								BehaviourManager::Get().SpecificBroadCastBehaviourMessage(e.self, "OnCollisionExit", e);
								break;
							}
						}
						else if constexpr (std::is_same_v<T, TriggerInfo>)
						{
							switch (e.phase)
							{
							case TriggerPhase::Enter:
								BehaviourManager::Get().SpecificBroadCastBehaviourMessage(e.self, "OnTriggerEnter", e);
								break;
							case TriggerPhase::Exit:
								BehaviourManager::Get().SpecificBroadCastBehaviourMessage(e.self, "OnTriggerExit", e);
								break;
							}
						}
					}, ev);
			}
		});


	if (EditorRegistry::g_editor_scene_playing
		&& !EditorRegistry::g_editor_scene_pause)
	{
		BehaviourManager::Get().BroadCastBehaviourMessage("Update");
		BehaviourManager::Get().BroadCastBehaviourMessage("LateUpdate");
	}

	RenderManager::Get().BeginFrame();
	RenderManager::Get().Render();
	ImGuiEditorContext::Get().BeginFrame();
	ImGuiEditorContext::Get().Render();
	ImGuiEditorContext::Get().EndFrame();
	RenderManager::Get().EndFrame();


	if (EditorRegistry::g_editor_scene_playing
		&& !EditorRegistry::g_editor_scene_pause)
	{
		ObjectManager::Get().UpdateInternalTimer(dt);
		BehaviourManager::Get().DisableBehaviours();
	}
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

#ifdef _DEBUG
int main()
#else
int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
#endif
{
#ifdef _DEBUG
	HINSTANCE hInstance = GetModuleHandle(nullptr);
#endif

	App app{ hInstance, L"MMMEditor",1600,900 };
	GlobalRegistry::g_pApp = &app;

	app.OnInitialize.AddListener<&Initialize>();
	app.OnUpdate.AddListener<&Update>();
	app.OnRelease.AddListener<&Release>();
	app.Run();

	return 0;
}
