#pragma once
#include "Export.h"
#include <string>
#include <vector>

namespace MMMEngine::Utility
{
	class MMMENGINE_API StringHelper
	{
	public:
		static std::vector<std::string> Split(const std::string str, char delim);
		static std::wstring StringToWString(const std::string& str);
		static std::string WStringToString(const std::wstring& wstr);
		static std::wstring ExtractFileFormat(const std::wstring& filepath);
		static std::wstring ExtractFileName(const std::wstring& filepath);
		static std::string CP949ToUTF8(const std::string& cp949Str);
	};
}
