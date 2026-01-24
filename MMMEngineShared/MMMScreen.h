#pragma once
#include "GlobalRegistry.h"
#include "DisplayMode.h"
#include "SimpleMath.h"
#include "RenderManager.h"
#include "App.h"
#include <cassert>

namespace MMMEngine::Screen
{
	inline void SetResolution(int width, int height) { assert(GlobalRegistry::g_pApp && "글로벌 레지스트리에 Application이 등록되어있지 않습니다!");  GlobalRegistry::g_pApp->SetWindowSize(width, height); }
	inline void SetResolution(int width, int height, DisplayMode mode)
	{
		assert(GlobalRegistry::g_pApp && "글로벌 레지스트리에 Application이 등록되어있지 않습니다!");
		RenderManager::Get().ResizeSceneSize(width, height, width, height);
		GlobalRegistry::g_pApp->SetDisplayMode(mode);
	}

	inline std::vector<Resolution> GetResolutions()
	{
		assert(GlobalRegistry::g_pApp && "글로벌 레지스트리에 Application이 등록되어있지 않습니다!");
		return GlobalRegistry::g_pApp->GetCurrentMonitorResolutions();
	}

	inline void SetResizable(bool isResizable)
	{
		assert(GlobalRegistry::g_pApp && "글로벌 레지스트리에 Application이 등록되어있지 않습니다!");
		GlobalRegistry::g_pApp->SetResizable(isResizable);
	}
}
