#pragma once
#include "GlobalRegistry.h"
#include <cassert>

namespace MMMEngine::Application
{
	inline void Quit() { assert(g_pApp && "글로벌 레지스트리에 Application이 등록되어있지 않습니다!"); g_pApp->Quit(); }
}