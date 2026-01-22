#include "TextResource.h"
#include "StringHelper.h"
#include <rttr/registration.h>
#include <rttr/registration_friend.h>
#include <filesystem>
#include <fstream>
#include "json/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;
using namespace MMMEngine::Utility;

RTTR_REGISTRATION
{
    using namespace rttr;
    using namespace MMMEngine;

    registration::class_<TextResource>("TextResource")
        .property_readonly("Buffer",&TextResource::GetBuffer);
}

bool MMMEngine::TextResource::LoadFromFilePath(const std::wstring& filePath)
{
    std::ifstream inFile(StringHelper::WStringToString(filePath),std::ios_base::binary);

    if (!inFile.is_open())
    {
        return false;
    }

    std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(inFile)),
        std::istreambuf_iterator<char>());
    inFile.close();

    m_buffer = std::move(buffer);

    return !m_buffer.empty();
}

const std::vector<uint8_t>& MMMEngine::TextResource::GetBuffer()
{
    return m_buffer;
}
