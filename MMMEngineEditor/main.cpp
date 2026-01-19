#include "RenderManager.h"
#include "BehaviourManager.h"
#include "SceneManager.h"
#include "ObjectManager.h"
#include "ProjectManager.h"

#include "StringHelper.h"
#include "ImGuiEditorContext.h"

using namespace MMMEngine;
using namespace MMMEngine::Utility;

void Init()
{
	auto app = GlobalRegistry::g_pApp;
	auto hwnd = app->GetWindowHandle();
	auto windowInfo = app->GetWindowInfo();

	InputManager::Get().StartUp(hwnd);
	app->OnWindowSizeChanged.AddListener<InputManager, &InputManager::HandleWindowResize>(&InputManager::Get());
	
	TimeManager::Get().StartUp();

	// ������ �״� ������Ʈ �켱 Ȯ��
	EditorRegistry::g_editor_project_loaded = ProjectManager::Get().Boot();
	if (EditorRegistry::g_editor_project_loaded)
	{
		// �����ϴ� ��� ���� ó������ ��ŸƮ
		auto currentProject = ProjectManager::Get().GetActiveProject();
		SceneManager::Get().StartUp(currentProject.ProjectRootFS().generic_wstring() + L"/Assets/Scenes", currentProject.lastSceneIndex, true);
		app->SetWindowTitle(L"MMMEditor [ " + Utility::StringHelper::StringToWString(currentProject.rootPath) + L" ]");
	}

	ObjectManager::Get().StartUp();
	BehaviourManager::Get().StartUp();

	RenderManager::Get().StartUp(hwnd, windowInfo.width, windowInfo.height);
	app->OnWindowSizeChanged.AddListener<RenderManager, &RenderManager::ResizeScreen>(&RenderManager::Get());

	ImGuiEditorContext::Get().Initialize(hwnd, RenderManager::Get().GetDevice(), RenderManager::Get().GetContext());
	app->OnBeforeWindowMessage.AddListener<ImGuiEditorContext, &ImGuiEditorContext::HandleWindowMessage>(&ImGuiEditorContext::Get());
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
		return;
	}
}

void Update()
{
	InputManager::Get().Update();
}

void Release()
{
	GlobalRegistry::g_pApp = nullptr;
}

int main()
{
	App app{ L"MMMEditor",1600,900 };
	GlobalRegistry::g_pApp = &app;

	app.OnInitialize.AddListener<&Init>();
	app.OnUpdate.AddListener<&Update>();
	app.Run();
}