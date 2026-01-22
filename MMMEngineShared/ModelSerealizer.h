#pragma once
#include "ExportSingleton.hpp"
#include "ResourceManager.h"

namespace MMMEngine {
	class StaticMesh;
	class SkeletalMesh;
	class MMMENGINE_API ModelSerealizer : public Utility::ExportSingleton<ModelSerealizer>
	{
	private:

	public:
		void Static_Serealize(StaticMesh* _mesh, std::wstring _path);		// 출력Path
		void Static_UnSerealize(StaticMesh* _mesh, std::wstring _path);		// 입력Path

		void Skeletal_Serealize(SkeletalMesh* _mesh, std::wstring _path);		// 출력Path
		void Skeletal_UnSerealize(SkeletalMesh* _mesh, std::wstring _path);		// 출력Path
	};
}


