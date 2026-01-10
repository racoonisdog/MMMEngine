#pragma once
#include "Singleton.hpp"

using namespace MMMEngine::Utility;

namespace MMMEngine
{
	class SceneManager : public Singleton<SceneManager>
	{
		void StartUp();
		void ShutDown();
		void Update();
	};
}