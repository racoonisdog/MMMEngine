#include "StringHelper.h"
#include <sstream>

std::vector<std::string> MMMEngine::Utility::StringHelper::Split(const std::string str, char delim)
{
    std::vector<std::string> tokens;
    std::istringstream stream(str);
    std::string token;
    while (std::getline(stream, token, delim))
    {
        if (!token.empty())
            tokens.push_back(token);
    }
    return tokens;
}
