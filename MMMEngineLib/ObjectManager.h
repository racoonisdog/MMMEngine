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
        friend class App;

        static inline thread_local bool m_isCreatingObject;
        static inline thread_local bool m_isDestroyingObject;

        mutable std::mutex m_mutex;
        std::vector<Object*> m_objects;
        std::vector<uint32_t> m_ptrGenerations;
        std::queue<uint32_t> m_freePtrIDs;
        std::vector<uint32_t> m_pendingDestroy;

        void ProcessPendingDestroy()
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            DestroyScope scope;

            for (uint32_t ptrID : m_pendingDestroy)
            {
                if (ptrID >= m_objects.size())
                    continue;

                Object* obj = m_objects[ptrID];
                if (!obj)
                    continue;

                delete obj;
                m_objects[ptrID] = nullptr;
                m_freePtrIDs.push(ptrID);
            }

            m_pendingDestroy.clear();
        }

        // === ID로 핸들 복원 (직렬화용) ===
        template<typename T>
        ObjectPtr<T> GetPtr(uint32_t ptrID)
        {
            if (ptrID >= m_objects.size())
                return ObjectPtr<T>();

            Object* obj = m_objects[ptrID];
            if (!obj || obj->IsDestroyed())
                return ObjectPtr<T>();

            T* typedObj = dynamic_cast<T*>(obj);
            if (!typedObj)
                return ObjectPtr<T>();

            uint32_t generation = m_ptrGenerations[ptrID];
            return ObjectPtr<T>(typedObj, ptrID, generation);
        }

        // === 디버깅 ===
        size_t GetObjectCount() const
        {
            size_t count = 0;
            for (const Object* obj : m_objects)
                if (obj && !obj->IsDestroyed())
                    count++;
            return count;
        }

    public:
        bool IsCreatingObject() const
        {
            return m_isCreatingObject;
        }

        bool IsDestroyingObject() const 
        { 
            return m_isDestroyingObject; 
        }

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

        // === 유효성 검증 ===
        bool IsValidPtr(uint32_t ptrID, uint32_t generation, const Object* ptr) const
        {
            if (ptrID >= m_objects.size())
                return false;

            if (m_objects[ptrID] != ptr)
                return false;

            if (m_ptrGenerations[ptrID] != generation)
                return false;

            if (ptr && ptr->IsDestroyed())
                return false;

            return true;
        }

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
                ptrID = static_cast<uint32_t>(m_objects.size());
                m_objects.push_back(newObj);
                m_ptrGenerations.push_back(0);
                generation = 0;
            }
            else
            {
                // 재사용 슬롯
                ptrID = m_freePtrIDs.front();
                m_freePtrIDs.pop();
                m_objects[ptrID] = newObj;
                generation = ++m_ptrGenerations[ptrID];
            }

            return ObjectPtr<T>(newObj, ptrID, generation);
        }

        // === 파괴 ===
        template<typename T>
        void Destroy(ObjectPtr<T> objPtr)
        {
            if (objPtr.IsValid())
            {
                std::lock_guard<std::mutex> lock(m_mutex);
                m_pendingDestroy.push_back(objPtr.m_handleID);
                objPtr.m_ptr->MarkDestory();
            }
        }

        ObjectManager() = default;
        ~ObjectManager()
        {
            DestroyScope scope;
            // 모든 객체 정리
            for (Object* obj : m_objects)
            {
                if (obj)
                {
                    delete obj;
                }
            }
        }
    };
} 