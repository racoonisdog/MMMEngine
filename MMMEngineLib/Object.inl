#pragma once

#include "ObjectManager.h"

namespace MMMEngine
{
    template<typename T, typename... Args>
    ObjectPtr<T> Object::CreateInstance(Args&&... args)
    {
        return ObjectManager::Get()
            ->CreateHandle<T>(std::forward<Args>(args)...);
    }
}