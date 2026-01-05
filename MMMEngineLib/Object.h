#pragma once
#include "rttr/type"
#include "rttr/registration_friend.h"
#include "GUID.h"
#include <cassert>

namespace MMMEngine
{
	class Object
	{
	private:
		friend class ObjectManager;
        friend class ObjectSerializer;

		template<typename T>
		friend class ObjectPtr;
        friend class ObjectPtrBase;
		
		RTTR_ENABLE()
		RTTR_REGISTRATION_FRIEND

		static uint64_t s_nextInstanceID;

		uint64_t		m_instanceID;
		std::string		m_name;
		GUID			m_guid;

		bool			m_isDestroyed = false;

		inline void		MarkDestroy() { m_isDestroyed = true; }

		inline void		SetGUID(const GUID& guid) { m_guid = guid; }
	protected:
        Object();
        virtual ~Object();
	public:
		Object(const Object&) = delete;
		Object& operator=(const Object&) = delete;

		template<typename T, typename ...Args>
        static ObjectPtr<T> CreateInstance(Args && ...args);

		static void Destroy(const ObjectPtrBase& objPtr, float delay = 0.0f);

		inline uint64_t				GetInstanceID() const { return m_instanceID; }

		inline const GUID&			GetGUID()		const { return m_guid; }

		inline const std::string&	GetName()		const { return m_name; }
		inline void					SetName(const std::string& name) { m_name = name; }

		inline const bool&			IsDestroyed()	const { return m_isDestroyed; }
	};
    
    class ObjectPtrBase
    {
    private:
        RTTR_ENABLE()
        RTTR_REGISTRATION_FRIEND

        template<typename T>
        friend class ObjectPtr;
        friend class ObjectManager;
        friend class ObjectSerializer;

        virtual Object* GetBase() const = 0;
    public:
        virtual uint32_t    GetPtrID() const = 0;
        virtual uint32_t    GetPtrGeneration() const = 0;
        virtual bool        IsValid() const = 0;
        
        virtual bool IsSameObject(const ObjectPtrBase& other) const = 0;

        virtual bool operator==(const ObjectPtrBase& other) const = 0;
        virtual bool operator!=(const ObjectPtrBase& other) const = 0;
        virtual bool operator==(std::nullptr_t) const = 0;
        virtual bool operator!=(std::nullptr_t) const = 0;
    };

    template<typename T>
    class ObjectPtr final : public ObjectPtrBase
    {
    private:
        RTTR_ENABLE(ObjectPtrBase)
        RTTR_REGISTRATION_FRIEND
        friend class ObjectManager;
        friend class ObjectSerializer;

        T* m_raw = nullptr;
        uint32_t m_ptrID = UINT32_MAX;
        uint32_t m_ptrGeneration = 0;

        virtual Object* GetBase() const override { return m_raw; }

        T* Get() const
        {
            if (!IsValid())
                return nullptr;
            return m_raw;
        }

        // private 생성자 - ObjectManager만 생성 가능
        ObjectPtr(T* raw, uint32_t id, uint32_t gen)
            : m_raw(raw)
            , m_ptrID(id)
            , m_ptrGeneration(gen)
        {
        }

    public:
        // 기본 생성자 (null handle)
        ObjectPtr() = default;

        // 복사/이동은 허용
        ObjectPtr(const ObjectPtr&) = default;
        ObjectPtr(ObjectPtr&&) noexcept = default;
        ObjectPtr& operator=(const ObjectPtr&) = default;
        ObjectPtr& operator=(ObjectPtr&&) noexcept = default;

        T& operator*() const 
        {
            T* raw = Get();
            assert(raw && "ObjectPtr의 역참조가 잘못되었습니다!");
            return *raw;
        }

        T* operator->() const
        {
            T* raw = Get();
            assert(raw && "유효하지 않은 ObjectPtr에 접근했습니다!");
            return raw;
        }

        virtual bool operator==(const ObjectPtrBase& other) const override
        {
            return m_ptrID == other.GetPtrID() &&
                m_ptrGeneration == other.GetPtrGeneration();
        }

        virtual bool operator!=(const ObjectPtrBase& other) const override
        {
            return !(*this == other);
        }

        // === nullptr 비교 (null 핸들 검사) ===
        virtual bool operator==(std::nullptr_t) const override
        {
            return m_ptrID == UINT32_MAX;  // null 핸들
        }

        virtual bool operator!=(std::nullptr_t) const override
        {
            return m_ptrID != UINT32_MAX;
        }

        virtual bool IsSameObject(const ObjectPtrBase& other) const override;

        explicit operator bool() const { return IsValid(); }

        virtual bool IsValid() const override;

        virtual uint32_t GetPtrID() const override { return m_ptrID; }
        virtual uint32_t GetPtrGeneration() const override { return m_ptrGeneration; }
    };
}

#include "Object.inl"