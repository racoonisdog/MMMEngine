#include "StringHelper.h"
#include <Windows.h>
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

std::wstring MMMEngine::Utility::StringHelper::StringToWString(const std::string& str)
{
    if (str.empty())
        return std::wstring();

    int size_needed = MultiByteToWideChar(CP_UTF8,
        0,
        str.c_str(),
        (int)str.length(), // NULL 종료 문자 제외
        NULL,
        0);

    if (size_needed == 0) {
        // 변환 실패
        return std::wstring();
    }

    std::wstring wstrTo(size_needed, 0); // 필요한 크기만큼 할당
    MultiByteToWideChar(CP_UTF8,
        0,
        str.c_str(),
        (int)str.length(),
        &wstrTo[0],
        size_needed);

    return wstrTo;
}

std::string MMMEngine::Utility::StringHelper::WStringToString(const std::wstring& wstr)
{
    if (wstr.empty())
        return std::string();

    // 필요한 string 크기 계산 (NULL 종료 문자 제외)
    // CP_UTF8를 사용하여 출력 문자열이 UTF-8임을 지정합니다.
    int size_needed = WideCharToMultiByte(CP_UTF8,
        0,
        wstr.c_str(),
        (int)wstr.length(), // NULL 종료 문자 제외
        NULL,
        0,
        NULL,
        NULL);

    if (size_needed == 0) {
        // 변환 실패
        return std::string();
    }

    // string 크기 조정 및 변환 수행
    std::string strTo(size_needed, 0); // 필요한 크기만큼 할당
    WideCharToMultiByte(CP_UTF8,
        0,
        wstr.c_str(),
        (int)wstr.length(),
        &strTo[0],
        size_needed,
        NULL,
        NULL);

    return strTo;
}

std::wstring MMMEngine::Utility::StringHelper::ExtractFileFormat(const std::wstring& filepath)
{
    size_t dotPos = filepath.find_last_of(L".");

    if (dotPos == std::wstring::npos)
    {
        return L"";
    }

    size_t lastSeparator = filepath.find_last_of(L"\\/");
    if (lastSeparator != std::wstring::npos && lastSeparator > dotPos)
    {
        return L"";
    }

    return filepath.substr(dotPos + 1);
}

std::wstring MMMEngine::Utility::StringHelper::ExtractFileName(const std::wstring& filepath)
{
    if (filepath.empty())
        return L"";

    size_t slashPos = filepath.find_last_of(L"/\\");
    std::wstring filename =
        (slashPos == std::wstring::npos)
        ? filepath
        : filepath.substr(slashPos + 1);

    size_t dotPos = filename.find_last_of(L'.');
    if (dotPos != std::wstring::npos)
    {
        return filename.substr(0, dotPos);
    }

    return filename;
}

std::string MMMEngine::Utility::StringHelper::CP949ToUTF8(const std::string& cp949Str)
{
    if (cp949Str.empty()) return "";

    // 1. CP949 -> WideChar (Unicode)
    int nwLen = MultiByteToWideChar(949, 0, cp949Str.c_str(), -1, NULL, 0);
    std::wstring wstr(nwLen, 0);
    MultiByteToWideChar(949, 0, cp949Str.c_str(), -1, &wstr[0], nwLen);

    // 2. WideChar -> UTF-8
    int nLen = WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, NULL, 0, NULL, NULL);
    std::string str(nLen, 0);
    WideCharToMultiByte(CP_UTF8, 0, wstr.c_str(), -1, &str[0], nLen, NULL, NULL);

    // 끝에 붙는 null terminator 제거
    if (!str.empty() && str.back() == '\0') str.pop_back();

    return str;
}
