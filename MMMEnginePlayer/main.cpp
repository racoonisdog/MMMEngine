#define NOMINMAX
#include <iostream>

#include "GlobalRegistry.h"
#include "PlayerRegistry.h"
#include "App.h"

#include "InputManager.h"
#include "ResourceManager.h"
#include "TimeManager.h"
#include "RenderManager.h"
#include "BehaviourManager.h"
#include "SceneManager.h"
#include "ObjectManager.h"
#include "PhysxManager.h"

#include "PhysicsSettings.h"
#include "ShaderInfo.h"

namespace fs = std::filesystem;
using namespace MMMEngine;
using namespace MMMEngine::Utility;
using namespace Microsoft::WRL;

void Initialize()
{
	fs::path cwd = fs::current_path();

	fs::path dataPath = cwd / "Data";

	//SetConsoleOutputCP(CP_UTF8);
	auto app = GlobalRegistry::g_pApp;
	auto hwnd = app->GetWindowHandle();
	auto windowInfo = app->GetWindowInfo();

	RenderManager::Get().StartUp(hwnd, windowInfo.width, windowInfo.height);
	RenderManager::Get().UseBackBufferDraw(true);
	InputManager::Get().StartUp(hwnd);
	app->OnWindowSizeChanged.AddListener<InputManager, &InputManager::HandleWindowResize>(&InputManager::Get());
	app->OnMouseWheelUpdate.AddListener<InputManager, &InputManager::HandleMouseWheelEvent>(&InputManager::Get());

	TimeManager::Get().StartUp();

	SceneManager::Get().StartUp(dataPath.generic_wstring() + L"/Assets/Scenes", 0);
	ObjectManager::Get().StartUp();

	PhysicsSettings::Get().StartUp(dataPath / "Settings");

	// 유저 스크립트 불러오기
	auto dllPath = cwd / "UserScripts.dll";
	BehaviourManager::Get().StartUp(dllPath.generic_u8string());

	// 리소스 매니저 부팅
	ResourceManager::Get().StartUp(dataPath.generic_wstring() + L"/");

	// 쉐이더 인포 시작하기
	ShaderInfo::Get().StartUp();

	app->OnWindowSizeChanged.AddListener<RenderManager, &RenderManager::ResizeSwapChainSize>(&RenderManager::Get());
	//app->OnWindowSizeChanged.AddListener<RenderManager, &RenderManager::ResizeSceneSize>(&RenderManager::Get());
	RenderManager::Get().ResizeSceneSize(1920, 1080);

	ComPtr<ID3D11Device> device = RenderManager::Get().GetDevice();
	ComPtr<ID3D11DeviceContext> context = RenderManager::Get().GetContext();

	PhysicX::Get().Initialize();
	SceneManager::Get().onSceneInitBefore.AddListenerLambda([]() {
		PhysxManager::Get().UnbindScene();
		PhysxManager::Get().BindScene(SceneManager::Get().GetCurrentSceneRaw());
		});
}

void Update()
{
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

	BehaviourManager::Get().InitializeBehaviours();

	TimeManager::Get().ConsumeFixedSteps([&](float fixedDt)
		{
			PhysxManager::Get().SetStep();
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

	BehaviourManager::Get().BroadCastBehaviourMessage("Update");
	BehaviourManager::Get().BroadCastBehaviourMessage("LateUpdate");

	PhysxManager::Get().ApplyInterpolation(TimeManager::Get().GetInterpolationAlpha());

	RenderManager::Get().BeginFrame();
	RenderManager::Get().Render();
	RenderManager::Get().EndFrame();

	ObjectManager::Get().UpdateInternalTimer(dt);
	BehaviourManager::Get().DisableBehaviours();
	ObjectManager::Get().ProcessPendingDestroy();
}

void Release()
{
	PhysxManager::Get().UnbindScene();
	GlobalRegistry::g_pApp = nullptr;
	RenderManager::Get().ShutDown();
	TimeManager::Get().ShutDown();
	InputManager::Get().ShutDown();

	SceneManager::Get().ShutDown();
	ObjectManager::Get().ShutDown();
	BehaviourManager::Get().ShutDown();

	fs::path cwd = fs::current_path();
}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	App app{ hInstance,L"MMMPlayer",1280,720 };
	GlobalRegistry::g_pApp = &app;

	app.OnInitialize.AddListener<&Initialize>();
	app.OnUpdate.AddListener<&Update>();
	app.OnRelease.AddListener<&Release>();
	app.Run();
}
