#pragma once
#include "ExportSingleton.hpp"
#include <string>
#include "ResourceManager.h"
#include <filesystem>

#include "Material.h"
#include "json/json.hpp"
#include <rttr/type>

namespace MMMEngine {
	class MMMENGINE_API MaterialSerializer : public Utility::ExportSingleton<MaterialSerializer>
	{
	private:
		PropertyValue property_from_json(const nlohmann::json& j);
		void to_json(nlohmann::json& j, const MMMEngine::PropertyValue& value);
	public:
		std::filesystem::path Serealize(Material* _in, std::wstring _path, std::wstring _name, int _index);		// _path는 출력path
		void UnSerealize(Material* _out, std::wstring _path);		// _path는 입력path
	};
}


