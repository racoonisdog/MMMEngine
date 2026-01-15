#pragma once
#include "ExportSingleton.hpp"
#include "Scene.h"

namespace MMMEngine
{
	class MMMENGINE_API SceneSerializer : public Utility::ExportSingleton<SceneSerializer>
	{
	public:
		void Serialize(const Scene& scene, std::wstring path);
	};
}