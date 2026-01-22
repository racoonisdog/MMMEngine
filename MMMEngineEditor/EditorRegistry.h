#pragma once
#include "GameObject.h"

namespace MMMEngine::EditorRegistry
{
	inline bool g_editor_window_console = true;
	inline bool g_editor_window_hierarchy = true;
	inline bool g_editor_window_inspector = true;
	inline bool g_editor_window_files = true;
	inline bool g_editor_window_scenelist = false;
	inline ObjPtr<GameObject> g_selectedGameObject = nullptr;

	inline bool g_editor_project_loaded = false;

	inline bool g_editor_scene_playing = false;     // 게임 진행중
	inline bool g_editor_scene_stop = true;			// 시간정지 인가 아닌가
}

