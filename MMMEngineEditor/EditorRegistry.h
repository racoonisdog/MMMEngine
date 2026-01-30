#pragma once
#include "GameObject.h"

namespace MMMEngine::EditorRegistry
{
	inline bool g_editor_window_console = true;
	inline bool g_editor_window_hierarchy = true;
	inline bool g_editor_window_inspector = true;
	inline bool g_editor_window_files = true;
	inline bool g_editor_window_scenelist = false;
	inline bool g_editor_window_sceneChange = false;
	inline bool g_editor_window_sceneName = false;
	inline bool g_editor_window_physicsSettings = false;
	inline bool g_editor_window_sceneView = true;
	inline bool g_editor_window_gameView = true;
	inline bool g_editor_window_playerBuild = false;
	inline bool g_editor_window_assimpLoader = false;
	inline ObjPtr<GameObject> g_selectedGameObject = nullptr;

	// 플레이 버튼 누르기 직전의 씬 번호
	inline size_t g_editor_scene_before_play_sceneID = static_cast<size_t>(-1);

	inline bool g_editor_project_loaded = false;

	inline bool g_editor_scene_playing = false;     // 게임 진행중
	inline bool g_editor_scene_pause = true;		// 시간정지 인가 아닌가
}

