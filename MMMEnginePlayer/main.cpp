#define NOMINMAX
#include <iostream>

#include "GlobalRegistry.h"
#include "PlayerRegistry.h"
#include "App.h"

#include "InputManager.h"
#include "TimeManager.h"
#include "ResourceManager.h"
#include "BehaviourManager.h"
#include "ObjectManager.h"
#include "SceneManager.h"

using namespace MMMEngine;
using namespace MMMEngine::Utility;


void Initialize()
{
	InputManager::Get().StartUp(GlobalRegistry::g_pApp->GetWindowHandle());
	GlobalRegistry::g_pApp->OnWindowSizeChanged.AddListener<InputManager, &InputManager::HandleWindowResize>(&InputManager::Get());

	SceneManager::Get().StartUp(L"Data", 0, false);
	BehaviourManager::Get().StartUp(L"UserScripts");
}

void Update()
{
	TimeManager::Get().BeginFrame();
	InputManager::Get().Update();

	float dt = TimeManager::Get().GetDeltaTime();

	//씬 변경 시
	if (SceneManager::Get().CheckSceneIsChanged())
	{
		ObjectManager::Get().UpdateInternalTimer(dt);
		BehaviourManager::Get().DisableBehaviours();
		ObjectManager::Get().ProcessPendingDestroy();
		BehaviourManager::Get().AllSortBehaviours();
		BehaviourManager::Get().AllBroadCastBehaviourMessage("OnSceneLoaded");
	}

	BehaviourManager::Get().InitializeBehaviours();    // Awake, OnEnable, Start �޽��� ���� (����������)

	TimeManager::Get().ConsumeFixedSteps([&](float fixedDt)
	{
		//PhysicsManager::Get()->PreSyncPhysicsWorld();
		//PhysicsManager::Get()->PreApplyTransform();
		BehaviourManager::Get().BroadCastBehaviourMessage("FixedUpdate");
		//PhysicsManager::Get()->Simulate(fixedDt);
		//PhysicsManager::Get()->ApplyTransform();
	});

	BehaviourManager::Get().BroadCastBehaviourMessage("Update");
	BehaviourManager::Get().BroadCastBehaviourMessage("LateUpdate");

	ObjectManager::Get().UpdateInternalTimer(dt);
	BehaviourManager::Get().DisableBehaviours();
	ObjectManager::Get().ProcessPendingDestroy();
}

void Release()
{
	SceneManager::Get().ShutDown();
	ObjectManager::Get().ShutDown();
	BehaviourManager::Get().ShutDown();
	GlobalRegistry::g_pApp = nullptr;
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