#pragma once
#include <imgui.h>
#include "Singleton.hpp"

namespace MMMEngine::Editor
{
	class GameViewWindow : public Utility::Singleton<GameViewWindow>
	{
	public: 
		void Render();
	private:
		ImVec2 m_lastSize;
	};
}