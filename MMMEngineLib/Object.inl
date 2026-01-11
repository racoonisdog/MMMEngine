#pragma once
#include "ObjectManager.h"
#include "Object.h"

namespace MMMEngine
{
    template<typename T, typename... Args>
    ObjPtr<T> Object::NewObject(Args&&... args)
    {
        return ObjectManager::Get()
            .NewObject<T>(std::forward<Args>(args)...);
    }

    template<typename T>
    bool ObjPtr<T>::IsValid() const
    {
        return ObjectManager::Get().IsValidPtr(m_ptrID, m_ptrGeneration, m_raw);
    }

    template<typename T>
    template<typename U>
    ObjPtr<U> ObjPtr<T>::Cast() const
    {
        if (U* casted = dynamic_cast<U*>(m_raw))
        {
            return ObjectManager::Get().GetPtrFast<U>(casted, m_ptrID, m_ptrGeneration);
        }

        return ObjPtr<U>();
    }

    template<typename T>
    template<typename U>
    ObjPtr<U> ObjPtr<T>::As() const
    {
        static_assert(std::is_base_of_v<Object, U>,
            "As<U>() : U는 Object를 상속받아야 합니다.");

        return ObjectManager::Get().GetPtr<U>(m_ptrID, m_ptrGeneration);
    }

    template<typename T>
    bool ObjPtr<T>::IsSameObject(const ObjPtrBase& other) const
    {
        if (m_ptrID != other.GetPtrID() ||
            m_ptrGeneration != other.GetPtrGeneration())
            return false;

        return IsValid() &&
            ObjectManager::Get().IsValidPtr(
                other.GetPtrID(),
                other.GetPtrGeneration(),
                other.GetRaw()
            );
    }


    template<typename T>
    ObjPtr<T> Object::FindObjectByType()
    {
        return ObjectManager::Get().FindObjectByType<T>();
    }

    template<typename T>
    std::vector<ObjPtr<T>> Object::FindObjectsByType()
    {
        return ObjectManager::Get().FindObjectsByType<T>();
    }

    template<typename T>
    ObjPtr<T> Object::SelfPtr(T* self)
    {
#ifdef _DEBUG
        static_assert(std::is_base_of_v<Object, T>, "SelfPtr<T> : T는 Object를 상속받아야합니다.");
        assert(static_cast<Object*>(self) == this && "SelfPtr의 인자가 자신이 아닙니다!");
#endif
        return ObjectManager::Get().GetPtrFast<T>(static_cast<T*>(this), m_ptrID, m_ptrGen);
    }
}

namespace rttr
{
#ifdef _MSC_VER
#pragma warning( push )
#pragma warning( disable : 4506 ) // warning C4506: 인라인 함수에 대한 정의가 없습니다.
#endif

    template<typename T>
    struct wrapper_mapper<std::reference_wrapper<MMMEngine::ObjPtr<T>>>
    {
        using type = std::reference_wrapper<MMMEngine::ObjPtr<T>>;
        using wrapped_type = MMMEngine::ObjPtr<T>&;

        static wrapped_type get(const type& obj) { return obj.get(); }

        static type create(wrapped_type value) { return std::ref(value); }
    };

#ifdef _MSC_VER
#pragma warning( pop )
#endif
}