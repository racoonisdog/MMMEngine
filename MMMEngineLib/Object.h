#pragma once
#include "rttr/type"
#include "rttr/registration_friend.h"
#include "GUID.h"

namespace MMMEngine
{
	class Object
	{
	private:
		friend class ObjectManager;

		template<typename T>
		friend class ObjectPtr;
		
		RTTR_ENABLE()
		RTTR_REGISTRATION_FRIEND

		static uint64_t s_nextInstanceID;

		uint64_t		m_instanceID;
		std::string		m_name;
		GUID			m_guid;

		bool			m_isDestroyed = false;

		inline void		MarkDestory() { m_isDestroyed = true; }

		inline void		SetGUID(const GUID& guid) { m_guid = guid; }
	protected:
        Object();
        virtual ~Object();
	public:
		Object(const Object&) = delete;
		Object& operator=(const Object&) = delete;

		template<typename T, typename ...Args>
        static ObjectPtr<T> CreateInstance(Args && ...args);

		static void Destroy(ObjectPtr<Object> objPtr);

		inline uint64_t				GetInstanceID() const { return m_instanceID; }

		inline const GUID&			GetGUID()		const { return m_guid; }

		inline const std::string&	GetName()		const { return m_name; }
		inline void					SetName(const std::string& name) { m_name = name; }

		inline const bool&			IsDestroyed()	const { return m_isDestroyed; }
	};
    
    class ObjectPtrBase
    {
    public:
        virtual Object* GetRaw() const = 0;
    };

    template<typename T>
    class ObjectPtr : public ObjectPtrBase
    {
    private:
        friend class ObjectManager;

        Object* GetRaw() const override { return m_ptr; }

        T* m_ptr = nullptr;
        uint32_t m_handleID = UINT32_MAX;
        uint32_t m_handleGeneration = 0;

        // private 생성자 - ObjectManager만 생성 가능
        ObjectPtr(T* ptr, uint32_t id, uint32_t gen)
            : m_ptr(ptr)
            , m_handleID(id)
            , m_handleGeneration(gen)
        {
        }

    public:
        // 기본 생성자 (null handle)
        ObjectPtr() = default;

        // 복사/이동은 허용
        ObjectPtr(const ObjectPtr&) = default;
        ObjectPtr(ObjectPtr&&) = default;
        ObjectPtr& operator=(const ObjectPtr&) = default;
        ObjectPtr& operator=(ObjectPtr&&) = default;

        T* Get() const
        {
            if (!IsValid())
                return nullptr;
            return m_ptr;
        }

        T& operator*() const { return *Get(); }
        T* operator->() const { return Get(); }

        bool operator==(const ObjectPtr<T>& other) const
        {
            // 같은 핸들 슬롯 + 같은 세대 = 같은 핸들
            return m_handleID == other.m_handleID &&
                m_handleGeneration == other.m_handleGeneration;
        }

        bool operator!=(const ObjectPtr<T>& other) const
        {
            return !(*this == other);
        }

        // === nullptr 비교 (null 핸들 검사) ===
        bool operator==(std::nullptr_t) const
        {
            return m_handleID == UINT32_MAX;  // null 핸들
        }

        bool operator!=(std::nullptr_t) const
        {
            return m_handleID != UINT32_MAX;
        }

        bool IsSameObject(const ObjectPtr<T>& other) const
        {
            T* p1 = Get();
            T* p2 = other.Get();
            return p1 && p2 && p1 == p2;
        }

        explicit operator bool() const { return IsValid(); }

        bool IsValid() const
        {
            return ObjectManager::Get()->IsValidHandle(m_handleID, m_handleGeneration, m_ptr);
        }

        uint32_t GetHandleID() const { return m_handleID; }
        uint32_t GetGeneration() const { return m_handleGeneration; }
    };


}

#include "Object.inl"