#define NOMINMAX
#include <iostream>

#include "GlobalRegistry.h"
#include "EditorRegistry.h"
#include "App.h"

#include "InputManager.h"
#include "ResourceManager.h"
#include "TimeManager.h"

using namespace MMMEngine;
using namespace MMMEngine::Utility;

void Init()
{
	InputManager::Get().StartUp(MMMEngine::g_pApp->GetWindowHandle());

	ResourceManager::Get().SetResolver(&Editor::g_resolver);
	ResourceManager::Get().SetBytesProvider(&Editor::g_filesProvider);
}

#include "Transform.h"

void Update()
{
	InputManager::Get().Update();
	static auto t1 = Object::NewObject<Transform>();

	TimeManager::Get().BeginFrame();
	TimeManager::Get().ConsumeFixedSteps([](float fixedDt)
		{

		});

	auto hInput = (InputManager::Get().GetKey(KeyCode::LeftArrow) ? -1.0f : 0.0f) + (InputManager::Get().GetKey(KeyCode::RightArrow) ? 1.0f : 0.0f);

	auto t1Pos = t1->GetLocalPosition();
	auto t1Vel = 24.0f * TimeManager::Get().GetDeltaTime() * hInput;
	t1->SetLocalPosition({ t1Pos.x + t1Vel ,0,0 });

	std::cout << t1->GetLocalPosition().x << std::endl;
}

int main()
{
	App app;
	MMMEngine::g_pApp = &app;

	app.OnInitialize.AddListener<&Init>();
	app.OnUpdate.AddListener<&Update>();
	app.Run();
}