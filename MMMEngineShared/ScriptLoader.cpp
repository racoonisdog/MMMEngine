#include "ScriptLoader.h"
#include <iostream>
#include "StringHelper.h"

using namespace rttr;
using namespace MMMEngine::Utility;

bool MMMEngine::ScriptLoader::LoadScriptDLL(const std::string& dllName)
{
    // 1. 기존 모듈이 있다면 확실히 정리
    if (m_pLoadedModule)
    {
        // 언로드 시도 및 결과 확인
        if (!m_pLoadedModule->unload()) {
            std::cerr << "Warning: Failed to unload previous script library. There might be leaked RTTR objects." << std::endl;
            // 여기서 중단하거나 더 강력한 청소 로직을 수행해야 함
        }
        m_pLoadedModule.reset();
    }

    // 2. 새로운 모듈 인스턴스 생성
    auto newModule = std::make_unique<rttr::library>(dllName.c_str());

    // 3. 로드 전 타입 이름 중복 체크 (선택 사항이나 권장)
    // 예: "Player" 타입이 이미 있다면 이전 언로드가 덜 된 것임

    if (!newModule->load())
    {
        std::cerr << "Load Error: " << newModule->get_error_string() << std::endl;
        return false;
    }

    m_pLoadedModule = std::move(newModule);
    return true;
}

MMMEngine::UnloadState MMMEngine::ScriptLoader::UnloadScript()
{
    if (m_pLoadedModule)
    {
        if (m_pLoadedModule->unload())
        {
            m_pLoadedModule.reset();
            m_pLoadedModule = nullptr;
            return UnloadState::UnloadSuccess;
        }
        else
        {
            return UnloadState::UnloadFail;
        }
    }

    return UnloadState::ScriptNotLoaded;
}

MMMEngine::ScriptLoader::~ScriptLoader()
{
    UnloadScript();
}
