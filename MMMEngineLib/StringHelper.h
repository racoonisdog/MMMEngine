#pragma once
#include <string>
#include <vector>

namespace MMMEngine::Utility
{
	class StringHelper
	{
	public:
		static std::vector<std::string> Split(const std::string str, char delim);
	};
}
