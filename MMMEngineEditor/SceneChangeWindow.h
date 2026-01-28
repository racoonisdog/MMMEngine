#pragma once
#include "Singleton.hpp"

namespace MMMEngine::Editor
{
	class SceneChangeWindow : public Utility::Singleton<SceneChangeWindow>
	{
	public:
		void Render();
	};
}
