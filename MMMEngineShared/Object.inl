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
    ObjPtr<T> Object::Instantiate(const ObjPtr<T>& original)
    {
        static_assert(std::is_base_of_v<Object, T>,
            "Instantiate<T>() : T는 Object를 상속받아야 합니다.");

        if constexpr (std::is_base_of_v<Component, T>)
        {
            ObjPtr<Component> base = Object::Instantiate(ObjPtr<Component>(original));
            if (!base.IsValid())
                return ObjPtr<T>();

            return base.Cast<T>();
        }
        else
        {
            static_assert(std::is_same_v<T, GameObject>,
                "Instantiate<T>() : GameObject 또는 Component 계열만 지원합니다.");

            ObjPtr<GameObject> cloned = Object::Instantiate(ObjPtr<GameObject>(original));
            if (!cloned.IsValid())
                return ObjPtr<T>();

            return cloned.Cast<T>();
        }
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
    inline bool ObjPtr<T>::Inject(const ObjPtrBase& ptr)
    {
		// null handle 주입 
        if (ptr.GetRaw() == nullptr
            && ptr.GetPtrID() == UINT32_MAX)
        {
            m_raw = nullptr;
            m_ptrID = UINT32_MAX;
            m_ptrGeneration = 0;
            return true;
        }

		// 타입 검사
        auto* base = static_cast<MMMEngine::Object*>(ptr.GetRaw());
        T* raw = dynamic_cast<T*>(base);

        if (raw != nullptr)
        {
            m_raw = raw;
            m_ptrGeneration = ptr.GetPtrGeneration();
            m_ptrID = ptr.GetPtrID();
            return true;
        }

        return false;
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
