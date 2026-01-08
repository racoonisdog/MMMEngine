#pragma once
#include "rttr/type"
#include "rttr/registration_friend.h"
#include "GUID.h"
#include <type_traits>
#include <cassert>

namespace MMMEngine
{
	class Object
	{
	private:
        RTTR_ENABLE()
        RTTR_REGISTRATION_FRIEND

		friend class ObjectManager;
        friend class ObjectSerializer;

		template<typename T>
		friend class ObjectPtr;
        friend class ObjectPtrBase;
		
		static uint64_t s_nextInstanceID;

        uint32_t        m_ptrID;
        uint32_t        m_ptrGen;
		uint64_t		m_instanceID;
		std::string		m_name;
		GUID			m_guid;

		bool			m_isDestroyed = false;

        inline void		MarkDestroy() { if (m_isDestroyed) return; m_isDestroyed = true; BeforeDestroy();  }
		inline void		SetGUID(const GUID& guid) { m_guid = guid; }
	protected:
        Object();
        virtual ~Object();

        template<typename T>
        ObjectPtr<T> SelfPtr(T* self);

        virtual void BeforeDestroy() {};
	public:
		Object(const Object&) = delete;
		Object& operator=(const Object&) = delete;

		template<typename T, typename ...Args>
        static ObjectPtr<T> CreatePtr(Args && ...args);

        template<typename T>
        static ObjectPtr<T> FindObjectByType();

        template<typename T>
        static std::vector<ObjectPtr<T>> FindObjectsByType();

        static void DontDestroyOnLoad(const ObjectPtrBase& objPtr);

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

        virtual void* GetRaw() const = 0;
    public:
        virtual void Reset() = 0;
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
        friend class ObjectManager;
        friend class ObjectSerializer;
        template<typename> friend class ObjectPtr;

        T* m_raw = nullptr;
        uint32_t m_ptrID = UINT32_MAX;
        uint32_t m_ptrGeneration = 0;

        virtual void* GetRaw() const override { return m_raw; }

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
        ObjectPtr(std::nullptr_t) { Reset(); }
        ObjectPtr() = default;

        // 복사/이동은 허용
        ObjectPtr(const ObjectPtr&) = default;
        ObjectPtr(ObjectPtr&&) noexcept = default;
        ObjectPtr& operator=(const ObjectPtr&) = default;
        ObjectPtr& operator=(ObjectPtr&&) noexcept = default;

        virtual void Reset() override { m_raw = nullptr; m_ptrID = UINT32_MAX; m_ptrGeneration = 0; }

        template<typename U,
            typename std::enable_if<std::is_base_of<T, U>::value, int>::type = 0>
        ObjectPtr(const ObjectPtr<U>& other)
            : m_raw(static_cast<T*>(other.m_raw))
            , m_ptrID(other.m_ptrID)
            , m_ptrGeneration(other.m_ptrGeneration)
        {
        }

        template<typename U,
            typename std::enable_if<std::is_base_of<T, U>::value, int>::type = 0>
        ObjectPtr& operator=(const ObjectPtr<U>& other)
        {
            m_raw = static_cast<T*>(other.m_raw);
            m_ptrID = other.m_ptrID;
            m_ptrGeneration = other.m_ptrGeneration;
            return *this;
        }

        /// <summary>
        /// 다이나믹 캐스트를 이용하여 빠르게 타입변환을 유도합니다. 
        /// 내부에서 주소유효성 검사 없이 타입변환된 ObjectPtr을 반환합니다.
        /// 타입변환이 유효하지 않은경우 내부 포인터가 nullptr이 됩니다.
        /// </summary>
        /// <typeparam name="U"></typeparam>
        /// <returns></returns>
        template<typename U>
        ObjectPtr<U> Cast() const
        {
            if (U* casted = dynamic_cast<U*>(m_raw))
            {
                return ObjectManager::Get().GetPtrFast<U>(casted, m_ptrID, m_ptrGeneration);
            }
            
            return ObjectPtr<U>();
        }

        /// <summary>
        /// 타입 변환을 시도한 후 성공 시 타입변환된 ObjectPtr을 반환합니다.
        /// As<U>()는 완전유효한 타입변환을 목표로 두기 떄문에
        /// 변환에 실패한 경우 컴파일타임에러가 일어납니다.
        /// </summary>
        /// <typeparam name="U"></typeparam>
        /// <returns></returns>
        template<typename U>
        ObjectPtr<U> As() const
        {
            static_assert(std::is_base_of_v<Object, U>,
                "As<U>() : U는 Object를 상속받아야 합니다.");

            return ObjectManager::Get().GetPtr<U>(m_ptrID, m_ptrGeneration);
        }

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

        bool operator==(T* other) const { return Get() == other; }
        bool operator!=(T* other) const { return Get() != other; }

        friend bool operator==(T* lhs, const ObjectPtr& rhs) { return rhs == lhs; }
        friend bool operator!=(T* lhs, const ObjectPtr& rhs) { return rhs != lhs; }

        bool operator==(const T* other) const { return Get() == other; }
        bool operator!=(const T* other) const { return Get() != other; }

        friend bool operator==(const T* lhs, const ObjectPtr& rhs) { return rhs == lhs; }
        friend bool operator!=(const T* lhs, const ObjectPtr& rhs) { return rhs != lhs; }

        virtual bool IsSameObject(const ObjectPtrBase& other) const override;

        explicit operator bool() const { return IsValid(); }

        virtual bool IsValid() const override;

        virtual uint32_t GetPtrID() const override { return m_ptrID; }
        virtual uint32_t GetPtrGeneration() const override { return m_ptrGeneration; }
    };
}

#include "Object.inl"