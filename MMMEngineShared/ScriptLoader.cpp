#include "ScriptLoader.h"
#include <iostream>
#include "StringHelper.h"

using namespace rttr;
using namespace MMMEngine::Utility;

bool MMMEngine::ScriptLoader::LoadScriptDLL(const std::string& dllName)
{
    if (m_pLoadedModule)
    {
        m_pLoadedModule->unload();
        m_pLoadedModule.reset();
    }

    m_pLoadedModule = std::make_unique<rttr::library>(dllName.c_str());

    if (!m_pLoadedModule->is_loaded() || !m_pLoadedModule->load())
    {
        std::cerr << m_pLoadedModule->get_error_string() << std::endl;
        return false;
    }

    return true;
}

MMMEngine::ScriptLoader::~ScriptLoader()
{
    if (m_pLoadedModule)
    {
        m_pLoadedModule->unload();
        m_pLoadedModule.reset();
    }
}
