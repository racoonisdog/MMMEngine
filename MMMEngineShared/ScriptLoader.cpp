#include "ScriptLoader.h"
#include <iostream>
#include "StringHelper.h"

using namespace rttr;
using namespace MMMEngine::Utility;

bool MMMEngine::ScriptLoader::LoadScriptDLL(const std::wstring& dllPath)
{
    //dllPath로 dll을 가져와서 복사

    //복사된 DLL의 이름을 개조 (복사본만 dll링킹 할 수 있도록)
    // *복사된 파일 이름 바꾸는 로직 -> UserScripts.dll = UserScripts_copy.dll*
    //std::string dllName = WStringToString(ExtractFileName(dllPath)) + "_copy";

    if (m_pLoadedModule)
    {
        m_pLoadedModule->unload();
        m_pLoadedModule.reset();
    }

    //m_pLoadedModule = std::make_unique<rttr::library>(dllName);

    if (!m_pLoadedModule->load())
    {
        std::cerr << m_pLoadedModule->get_error_string() << std::endl;
        return false;
    }

    return true;
}
