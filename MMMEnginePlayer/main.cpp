#define NOMINMAX
#include <iostream>

#include "GlobalRegistry.h"
#include "PlayerRegistry.h"
#include "App.h"

#include "MMMApplication.h"
#include "MMMTime.h"

#include "InputManager.h"
#include "TimeManager.h"
#include "ResourceManager.h"

using namespace MMMEngine;
using namespace MMMEngine::Utility;

void Initialize()
{
	InputManager::Get().StartUp(MMMEngine::g_pApp->GetWindowHandle());

	ResourceManager::Get().SetResolver(&Player::g_resolver);
	ResourceManager::Get().SetBytesProvider(&Player::g_bytes);
}

void Update()
{
	InputManager::Get().Update();
	TimeManager::Get().BeginFrame();
	TimeManager::Get().ConsumeFixedSteps([&](float fixedDt)
	{
		Application::SetTitle(L"fps : " + std::to_wstring(1.0f / Time::GetDeltaTime()));

		//PhysicsManager::Get()->PreSyncPhysicsWorld();
		//PhysicsManager::Get()->PreApplyTransform();
		//BehaviourManager::Get()->BroadCastBehaviourMessage("FixedUpdate");
		//PhysicsManager::Get()->Simulate(fixedDt);
		//PhysicsManager::Get()->ApplyTransform();
	});
}

void Render()
{

}

void Release()
{

}

int APIENTRY wWinMain(_In_ HINSTANCE hInstance,
	_In_opt_ HINSTANCE hPrevInstance,
	_In_ LPWSTR    lpCmdLine,
	_In_ int       nCmdShow)
{
	//if (!Player::g_pathMap.LoadFromFile(L"Data/PathMap.bin"))
	//	return -1;

	//if (!Player::g_assetIndex.LoadFromFile(L"Data/AssetIndex.bin"))
	//	return -2;

	//if (!Player::g_pak.Open(L"Data/assets.pak"))
	//	return -3;

	App app{ hInstance,L"MMMPlayer",1280,720 };
	MMMEngine::g_pApp = &app;

	app.OnInitialize.AddListener<&Initialize>();
	app.OnUpdate.AddListener<&Update>();
	app.OnRender.AddListener<&Render>();
	app.OnRelease.AddListener<&Release>();
	app.Run();
}