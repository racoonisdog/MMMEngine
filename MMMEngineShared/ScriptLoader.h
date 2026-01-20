#pragma once
#include <string>
#include "rttr/type"
#include "Export.h"

namespace MMMEngine
{
	class MMMENGINE_API ScriptLoader
	{
	private:
		std::unique_ptr<rttr::library> m_pLoadedModule;
	public:
		bool LoadScriptDLL(const std::string& dllName);
		~ScriptLoader();
	};
}