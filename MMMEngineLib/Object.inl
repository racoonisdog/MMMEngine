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
    bool MMMEngine::ObjectPtr<T>::IsValid() const
    {
        return ObjectManager::Get().IsValidPtr(m_ptrID, m_ptrGeneration, m_raw);
    }

    template<typename T>
    bool MMMEngine::ObjectPtr<T>::IsSameObject(const ObjectPtrBase& other) const
    {
        if (m_ptrID != other.GetPtrID() ||
            m_ptrGeneration != other.GetPtrGeneration())
            return false;

        return IsValid() &&
            ObjectManager::Get().IsValidPtr(
                other.GetPtrID(),
                other.GetPtrGeneration(),
                other.GetBase()
            );
    }
}