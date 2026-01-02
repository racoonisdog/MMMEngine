#pragma once
#include "Singleton.hpp"
#include "Object.h"
#include <vector>
#include <queue>

namespace MMMEngine
{
    class ObjectManager : public Singleton<ObjectManager>
    {
    private:
        friend class App;

        bool m_isCreatingObject = false;
        bool m_isDestroyingObject = false;

        std::vector<Object*> m_objects;
        std::vector<uint32_t> m_handleGenerations;
        std::queue<uint32_t> m_freeHandleIDs;
        std::vector<uint32_t> m_pendingDestroy;

        void ProcessPendingDestroy()
        {
            for (uint32_t handleID : m_pendingDestroy)
            {
                if (handleID >= m_objects.size())
                    continue;

                Object* obj = m_objects[handleID];
                if (!obj)
                    continue;

                delete obj;
                m_objects[handleID] = nullptr;
                m_freeHandleIDs.push(handleID);
            }

            m_pendingDestroy.clear();
        }

        // === ID로 핸들 복원 (직렬화용) ===
        template<typename T>
        ObjectPtr<T> GetHandle(uint32_t handleID)
        {
            if (handleID >= m_objects.size())
                return ObjectPtr<T>();

            Object* obj = m_objects[handleID];
            if (!obj || obj->IsDestroyed())
                return ObjectPtr<T>();

            T* typedObj = dynamic_cast<T*>(obj);
            if (!typedObj)
                return ObjectPtr<T>();

            uint32_t generation = m_handleGenerations[handleID];
            return ObjectPtr<T>(typedObj, handleID, generation);
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
            explicit CreationScope(ObjectManager* mgr)
                : m_mgr(mgr)
            {
                m_mgr->m_isCreatingObject = true;
            }

            ~CreationScope()
            {
                m_mgr->m_isCreatingObject = false;
            }

        private:
            ObjectManager* m_mgr;
        };

        class DestroyScope
        {
        public:
            DestroyScope(ObjectManager* mgr) : m_mgr(mgr)
            {
                m_mgr->m_isDestroyingObject = true;
            }
            ~DestroyScope()
            {
                m_mgr->m_isDestroyingObject = false;
            }
        private:
            ObjectManager* m_mgr;
        };

        // === 유효성 검증 ===
        bool IsValidHandle(uint32_t handleID, uint32_t generation, const Object* ptr) const
        {
            if (handleID >= m_objects.size())
                return false;

            if (m_objects[handleID] != ptr)
                return false;

            if (m_handleGenerations[handleID] != generation)
                return false;

            if (ptr && ptr->IsDestroyed())
                return false;

            return true;
        }

        template<typename T, typename... Args>
        ObjectPtr<T> CreateHandle(Args&&... args)
        {
            static_assert(std::is_base_of_v<Object, T>, "T must derive from Object");

            CreationScope scope(this);

            T* newObj = new T(std::forward<Args>(args)...);
            uint32_t handleID;
            uint32_t generation;

            if (m_freeHandleIDs.empty())
            {
                // 새 슬롯 할당
                handleID = static_cast<uint32_t>(m_objects.size());
                m_objects.push_back(newObj);
                m_handleGenerations.push_back(0);
                generation = 0;
            }
            else
            {
                // 재사용 슬롯
                handleID = m_freeHandleIDs.front();
                m_freeHandleIDs.pop();
                m_objects[handleID] = newObj;
                generation = ++m_handleGenerations[handleID];
            }

            return ObjectPtr<T>(newObj, handleID, generation);
        }

        // === 파괴 ===
        template<typename T>
        void Destroy(ObjectPtr<T> objPtr)
        {
            if (objPtr.IsValid())
            {
                m_pendingDestroy.push_back(objPtr.m_handleID);
                objPtr.m_ptr->MarkDestory();
            }
        }

        ObjectManager() = default;
        ~ObjectManager()
        {
            // 모든 객체 정리
            for (Object* obj : m_objects)
            {
                if (obj)
                {
                    DestroyScope scope(this);
                    delete obj;
                }
                    
            }
        }
    };
} 