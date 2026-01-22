#pragma once
#include "rttr/type"
#include "rttr/registration_friend.h"
#include "MUID.h"
#include <type_traits>
#include <cassert>

#include "Export.h"

namespace MMMEngine
{
    class MMMENGINE_API Object
	{
	private:
        RTTR_ENABLE()
        RTTR_REGISTRATION_FRIEND

		friend class ObjectManager;
        friend class SceneSerializer;

        template<typename T>
        friend struct rttr::wrapper_mapper;

		template<typename T>
		friend class ObjPtr;
        friend class ObjPtrBase;
		
		static uint64_t s_nextInstanceID;

        uint32_t        m_ptrID;
        uint32_t        m_ptrGen;
		uint64_t		m_instanceID;
		std::string		m_name;
		Utility::MUID	m_muid;

		bool			m_isDestroyed = false;

        inline void		MarkDestroy() { if (m_isDestroyed) return; m_isDestroyed = true; Dispose();  }
		inline void		SetMUID(const Utility::MUID& muid) { m_muid = muid; }
	protected:
        Object();
        virtual ~Object();

        template<typename T>
        ObjPtr<T> SelfPtr(T* self);     // 자신의 코드에서 자신의 ObjPt을 찾기위한 방법 -> this와 같음, 유효한 범위 : 생성자 호출 이후, 소멸자 호출 이전

        virtual void Construct() {};    // SelfPtr(this)를 사용하기 위한 생성자 호출 이후 이벤트 -> 다른 대상에 대한 참조 연결에 사용
        virtual void Dispose() {};      // SelfPtr(this)를 사용하기 위한 소멸자 호출 이전 이벤트 -> 다른 대상에 대한 참조 끊기에 사용
	public:
		Object(const Object&) = delete;
		Object& operator=(const Object&) = delete;

		template<typename T = Object, typename ...Args>
        static ObjPtr<T> NewObject(Args && ...args);

        template<typename T>
        static ObjPtr<T> FindObjectByType();

        template<typename T>
        static std::vector<ObjPtr<T>> FindObjectsByType();

        static void DontDestroyOnLoad(const ObjPtrBase& objPtr);

		static void Destroy(const ObjPtrBase& objPtr, float delay = 0.0f);

		inline uint64_t				GetInstanceID() const { return m_instanceID; }

		inline const Utility::MUID&			GetMUID()		const { return m_muid; }

		inline const std::string&	GetName()		const { return m_name; }
		inline void					SetName(const std::string& name) { m_name = name; }

		inline const bool&			IsDestroyed()	const { return m_isDestroyed; }
	};
    
    class MMMENGINE_API ObjPtrBase
    {
    private:
        RTTR_ENABLE()
        RTTR_REGISTRATION_FRIEND
        template<typename T>
        friend class ObjPtr;
        friend class ObjectManager;
        friend class ObjectSerializer;

        virtual void* GetRaw() const = 0;
    public:
        virtual void        Reset() = 0;
        virtual uint32_t    GetPtrID() const = 0;
        virtual uint32_t    GetPtrGeneration() const = 0;
        virtual bool        IsValid() const = 0;
        
        virtual bool IsSameObject(const ObjPtrBase& other) const = 0;

        virtual bool operator==(const ObjPtrBase& other) const = 0;
        virtual bool operator!=(const ObjPtrBase& other) const = 0;
        virtual bool operator==(std::nullptr_t) const = 0;
        virtual bool operator!=(std::nullptr_t) const = 0;
    };

    template<typename T>
    class ObjPtr final : public ObjPtrBase
    {
    private:
        template<typename>
        friend struct rttr::wrapper_mapper;
        template<typename U>
        friend struct std::hash;
        friend class ObjectManager;
        friend class ObjectSerializer;
        friend class PhysxManager;
        template<typename> friend class ObjPtr;

        T*          m_raw               = nullptr;
        uint32_t    m_ptrID             = UINT32_MAX;
        uint32_t    m_ptrGeneration     = 0;

        virtual void* GetRaw() const override { return m_raw; }

        T* Get() const
        {
            if (!IsValid())
                return nullptr;
            return m_raw;
        }

        // private 생성자 - ObjectManager만 생성 가능
        ObjPtr(T* raw, uint32_t id, uint32_t gen)
            : m_raw(raw)
            , m_ptrID(id)
            , m_ptrGeneration(gen)
        {
        }

    public:
        // 기본 생성자 (null handle)
        ObjPtr(std::nullptr_t) { Reset(); }
        ObjPtr() = default;

        // 복사/이동은 허용
        ObjPtr(const ObjPtr&) = default;
        ObjPtr(ObjPtr&&) noexcept = default;
        ObjPtr& operator=(const ObjPtr&) = default;
        ObjPtr& operator=(ObjPtr&&) noexcept = default;

        virtual void Reset() override { m_raw = nullptr; m_ptrID = UINT32_MAX; m_ptrGeneration = 0; }

        template<typename U,
            typename std::enable_if<std::is_base_of<T, U>::value, int>::type = 0>
        ObjPtr(const ObjPtr<U>& other)
            : m_raw(static_cast<T*>(other.m_raw))
            , m_ptrID(other.m_ptrID)
            , m_ptrGeneration(other.m_ptrGeneration)
        {
        }

        template<typename U,
            typename std::enable_if<std::is_base_of<T, U>::value, int>::type = 0>
        ObjPtr& operator=(const ObjPtr<U>& other)
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
        ObjPtr<U> Cast() const;

        /// <summary>
        /// 타입 변환을 시도한 후 성공 시 타입변환된 ObjectPtr을 반환합니다.
        /// As<U>()는 완전유효한 타입변환을 목표로 두기 때문에
        /// 변환에 실패한 경우 컴파일타임에러가 일어납니다.
        /// </summary>
        /// <typeparam name="U"></typeparam>
        /// <returns></returns>
        template<typename U>
        ObjPtr<U> As() const;

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

        virtual bool operator==(const ObjPtrBase& other) const override
        {
            return m_ptrID == other.GetPtrID() &&
                m_ptrGeneration == other.GetPtrGeneration();
        }

        virtual bool operator!=(const ObjPtrBase& other) const override
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

        friend bool operator==(T* lhs, const ObjPtr& rhs) { return rhs == lhs; }
        friend bool operator!=(T* lhs, const ObjPtr& rhs) { return rhs != lhs; }

        bool operator==(const T* other) const { return Get() == other; }
        bool operator!=(const T* other) const { return Get() != other; }

        friend bool operator==(const T* lhs, const ObjPtr& rhs) { return rhs == lhs; }
        friend bool operator!=(const T* lhs, const ObjPtr& rhs) { return rhs != lhs; }

        virtual bool IsSameObject(const ObjPtrBase& other) const override;

        explicit operator bool() const { return IsValid(); }

        virtual bool IsValid() const override;

        virtual uint32_t GetPtrID() const override { return m_ptrID; }
        virtual uint32_t GetPtrGeneration() const override { return m_ptrGeneration; }
    };
}

namespace rttr
{
    template <typename T>
    struct wrapper_mapper<MMMEngine::ObjPtr<T>>
    {
        using type = MMMEngine::ObjPtr<T>;
        using wrapped_type = T*; // get()의 반환 타입과 반드시 일치해야 함 :contentReference[oaicite:2]{index=2}

        static wrapped_type get(const type& obj)
        {
            // friend wrapper_mapper 선언되어 있어서 private Get() 접근 가능
            return obj.Get(); // 유효성 검사 포함
        }

        template <typename U>
        static MMMEngine::ObjPtr<U> convert(const type& source, bool& ok)
        {
            // ObjPtr<T> -> ObjPtr<U> 변환 지원 (예: Base -> Derived)
            if (auto casted = source.Cast<U>())
            {
                ok = true;
                return casted;
            }

            ok = false;
            return MMMEngine::ObjPtr<U>();
        }
    };
} // namespace rttr

namespace std
{
    template<typename T>
    struct hash<MMMEngine::ObjPtr<T>>
    {
        size_t operator()(const MMMEngine::ObjPtr<T>& p) const noexcept
        {
            size_t h1 = std::hash<uint32_t>{}(p.GetPtrID());
            size_t h2 = std::hash<uint32_t>{}(p.GetPtrGeneration());
            return h1 ^ (h2 + 0x9e3779b97f4a7c15ull + (h1 << 6) + (h1 >> 2));
        }
    };
}

#include "Object.inl"