#pragma once
#include "Singleton.hpp"
#include "Object.h"
#include <vector>
#include <queue>
#include <mutex>

namespace MMMEngine
{
    class ObjectManager : public Singleton<ObjectManager>
    {
    private:

        struct ObjectPtrInfo
        {
            Object* raw = nullptr;
            uint32_t ptrGenerations = 0;

            float destroyRemainTime = -1.0f;
            bool destroyScheduled = false;
            bool destroyPending = false;     
        };

        friend class App;

        static inline thread_local bool m_isCreatingObject;
        static inline thread_local bool m_isDestroyingObject;

        mutable std::mutex m_mutex;

        std::vector<ObjectPtrInfo> m_objectPtrInfos;
        std::queue<uint32_t> m_freePtrIDs;

        std::vector<uint32_t> m_delayedDestroy;   //파괴 예약 ID
        std::vector<uint32_t> m_pendingDestroy;   //완전 파괴 ID

        void Update(float deltaTime);

        void ProcessPendingDestroy();

        // PtrID로 Ptr 복원 (직렬화용)
        template<typename T>
        ObjectPtr<T> GetPtr(uint32_t ptrID)
        {
            if (ptrID >= m_objectPtrInfos.size())
                return ObjectPtr<T>();

            Object* obj = m_objectPtrInfos[ptrID].raw;
            if (!obj || obj->IsDestroyed())
                return ObjectPtr<T>();

            T* typedObj = dynamic_cast<T*>(obj);
            if (!typedObj)
                return ObjectPtr<T>();

            uint32_t generation = m_objectPtrInfos[ptrID].ptrGenerations;
            return ObjectPtr<T>(typedObj, ptrID, generation);
        }

        size_t GetObjectCount() const;
    public:
        bool IsCreatingObject() const;

        bool IsDestroyingObject() const;

        // RAII 스코프 -> Object 스택 생성 막기용
        class CreationScope
        {
        public:
            CreationScope()
            {
                ObjectManager::m_isCreatingObject = true;
            }

            ~CreationScope()
            {
                ObjectManager::m_isCreatingObject = false;
            }
        };

        class DestroyScope
        {
        public:
            DestroyScope()
            {
                ObjectManager::m_isDestroyingObject = true;
            }

            ~DestroyScope()
            {
                ObjectManager::m_isDestroyingObject = false;
            }
        };

        bool IsValidPtr(uint32_t ptrID, uint32_t generation, const Object* ptr) const;

        template<typename T, typename... Args>
        ObjectPtr<T> CreatePtr(Args&&... args)
        {
            static_assert(std::is_base_of_v<Object, T>, "T는 반드시 Object를 상속받아야 합니다.");
            static_assert(!std::is_abstract_v<T>, "추상적인 Object는 만들 수 없습니다.");

            std::lock_guard<std::mutex> lock(m_mutex);

            CreationScope scope;

            T* newObj = new T(std::forward<Args>(args)...);
            uint32_t ptrID;
            uint32_t generation;

            if (m_freePtrIDs.empty())
            {
                // 새 슬롯 할당
                ptrID = static_cast<uint32_t>(m_objectPtrInfos.size());
                m_objectPtrInfos.push_back({ newObj,0,0 });
                generation = 0;
            }
            else
            {
                // 재사용 슬롯
                ptrID = m_freePtrIDs.front();
                m_freePtrIDs.pop();
                m_objectPtrInfos[ptrID].raw = newObj;
                generation = ++m_objectPtrInfos[ptrID].ptrGenerations;
            }

            return ObjectPtr<T>(newObj, ptrID, generation);
        }

        void Destroy(const ObjectPtrBase& objPtr, float delayTime = 0.0f);

        ObjectManager() = default;
        ~ObjectManager();
    };
} 