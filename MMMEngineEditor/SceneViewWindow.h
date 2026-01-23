#pragma once
#include "Singleton.hpp"

namespace MMMEngine::Editor
{
	class SceneViewWindow : public Utility::Singleton<SceneViewWindow>
	{
	public:
		void Render();
	};
}