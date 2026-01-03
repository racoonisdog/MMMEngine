#pragma once
#include "ObjectManager.h"

namespace MMMEngine
{
    template<typename T, typename... Args>
    ObjectPtr<T> Object::CreateInstance(Args&&... args)
    {
        return ObjectManager::Get()
            .CreatePtr<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    void MMMEngine::Object::Destroy(MMMEngine::ObjectPtr<T> objPtr)
    {
        ObjectManager::Get().Destroy(objPtr);
    }
}