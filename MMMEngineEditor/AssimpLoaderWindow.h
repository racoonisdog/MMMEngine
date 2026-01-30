#pragma once
#include "Singleton.hpp"

namespace MMMEngine::Editor
{
	class AssimpLoaderWindow : public Utility::Singleton<AssimpLoaderWindow>
	{
	public:
		void Render();
	};
}
