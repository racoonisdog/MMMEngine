#pragma once
#include "ExportSingleton.hpp"
#include "ResourceManager.h"
#include <filesystem>

namespace MMMEngine {
	class StaticMesh;
	class MMMENGINE_API ResourceSerializer : public Utility::ExportSingleton<ResourceSerializer>
	{
	public:
		//TODO::파일들 이름 정하게하기
		std::filesystem::path Serialize_StaticMesh(const StaticMesh* _in, std::wstring _path, std::wstring _name);		// 출력Path
		void DeSerialize_StaticMesh(StaticMesh* _out, std::wstring _path);			// 입력Path
	};
}
