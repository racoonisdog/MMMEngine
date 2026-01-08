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
        };

        friend class App;

        static inline thread_local bool m_isCreatingObject;
        static inline thread_local bool m_isDestroyingObject;

        std::vector<ObjectPtrInfo> m_objectPtrInfos;
        std::queue<uint32_t> m_freePtrIDs;

        std::vector<uint32_t> m_delayedDestroy;   //파괴 예약 ID
        std::vector<uint32_t> m_pendingDestroy;   //완전 파괴 ID

        void Update(float deltaTime);

        void ProcessPendingDestroy();
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

        bool IsValidPtr(uint32_t ptrID, uint32_t generation, const void* ptr) const;

        // SelfPtr<T>의 빠른 구현을 위한 함수, 절대 외부 호출하지 말 것
        template<typename T>
        ObjectPtr<T> GetPtrFast(Object* raw, uint32_t ptrID, uint32_t ptrGen)
        {
            return ObjectPtr<T>(static_cast<T*>(raw), ptrID, ptrGen);
        }

        template<typename T>
        ObjectPtr<T> GetPtr(uint32_t ptrID, uint32_t ptrGen)
        {
            if (ptrID >= m_objectPtrInfos.size())
                return ObjectPtr<T>();

            Object* obj = m_objectPtrInfos[ptrID].raw;
            if (!obj || obj->IsDestroyed())
                return ObjectPtr<T>();

#ifndef NDEBUG
            T* typedObj = dynamic_cast<T*>(obj);
            assert(typedObj && "GetPtr<T>: 타입 불일치! ptrID에 들어있는 실제 타입을 확인하세요.");
#endif
            if(m_objectPtrInfos[ptrID].ptrGenerations != ptrGen)
                return ObjectPtr<T>();

            return ObjectPtr<T>(static_cast<T*>(obj), ptrID, ptrGen);
        }

        template<typename T>
        ObjectPtr<T> FindObjectByType()
        {
            for (uint32_t i = 0; i < m_objectPtrInfos.size(); ++i)
            {
                auto& info = m_objectPtrInfos[i];
                if (!IsValidPtr(i, info.ptrGenerations, info.raw))
                    continue;

                auto obj = info.raw;
                if (T* castedObj = dynamic_cast<T*>(obj))
                {
                    return ObjectPtr<T>(castedObj, i, info.ptrGenerations);
                }
            }

            return ObjectPtr<T>();
        }

        template<typename T>
        std::vector<ObjectPtr<T>> FindObjectsByType()
        {
            std::vector<ObjectPtr<T>> objects;

            for (uint32_t i = 0; i < m_objectPtrInfos.size(); ++i)
            {
                auto& info = m_objectPtrInfos[i];
                if (!IsValidPtr(i, info.ptrGenerations, info.raw))
                    continue;

                auto obj = info.raw;
                if (T* castedObj = dynamic_cast<T*>(obj))
                {
                    objects.emplace_back(ObjectPtr<T>(castedObj, i, info.ptrGenerations));
                }
            }

            return objects;
        }

        template<typename T, typename... Args>
        ObjectPtr<T> CreatePtr(Args&&... args)
        {
            static_assert(std::is_base_of_v<Object, T>, "T는 반드시 Object를 상속받아야 합니다.");
            static_assert(!std::is_abstract_v<T>, "추상적인 Object는 만들 수 없습니다.");

            CreationScope scope;

            T* newObj = new T(std::forward<Args>(args)...);
            uint32_t ptrID;
            uint32_t ptrGen;

            if (m_freePtrIDs.empty())
            {
                // 새 슬롯 할당
                ptrID = static_cast<uint32_t>(m_objectPtrInfos.size());
                m_objectPtrInfos.push_back({ newObj,0,0 });
                ptrGen = 0;
            }
            else
            {
                // 재사용 슬롯
                ptrID = m_freePtrIDs.front();
                m_freePtrIDs.pop();
                m_objectPtrInfos[ptrID].raw = newObj;
                ptrGen = ++m_objectPtrInfos[ptrID].ptrGenerations;
            }
            auto baseObj = static_cast<Object*>(newObj);
            baseObj->m_ptrID = ptrID;
            baseObj->m_ptrGen = ptrGen;
            return ObjectPtr<T>(newObj, ptrID, ptrGen);
        }

        void Destroy(const ObjectPtrBase& objPtr, float delayTime = 0.0f);

        ObjectManager() = default;
        ~ObjectManager();
    };
} 