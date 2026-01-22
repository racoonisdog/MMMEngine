#include "PhysicsEventCallback.h"

//물리 접촉 이벤트 수집
void MMMEngine::PhysXSimulationCallback::onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    physx::PxActor* actorA = pairHeader.actors[0];
    physx::PxActor* actorB = pairHeader.actors[1];

    for (physx::PxU32 i = 0; i < nbPairs; ++i)
    {
        const physx::PxContactPair& cp = pairs[i];

        // 삭제된 shape 관련이면 스킵하도록
        if (cp.flags & (physx::PxContactPairFlag::eREMOVED_SHAPE_0 |
            physx::PxContactPairFlag::eREMOVED_SHAPE_1))
        {
            continue;
        }

        // FOUND/PERSISTS/LOST 중 아무것도 아니면 스킵
        const bool hasTouchEvent =
            (cp.events & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND) ||
            (cp.events & physx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS) ||
            (cp.events & physx::PxPairFlag::eNOTIFY_TOUCH_LOST);

        if (!hasTouchEvent)
            continue;

        physx::PxShape* shapeA = cp.shapes[0];
        physx::PxShape* shapeB = cp.shapes[1];

        //순서보장이 안될수도 있으니 안전코드
        if (shapeA && shapeA->getActor() != actorA)
            std::swap(shapeA, shapeB);

        m_contacts.emplace_back(actorA, actorB, shapeA, shapeB, static_cast<physx::PxU32>(cp.events));
    }
}

void MMMEngine::PhysXSimulationCallback::onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count)
{
    std::lock_guard<std::mutex> lock(m_mtx);

    for (physx::PxU32 i = 0; i < count; ++i)
    {
        const auto& p = pairs[i];

        if (p.flags & (physx::PxTriggerPairFlag::eREMOVED_SHAPE_TRIGGER |
            physx::PxTriggerPairFlag::eREMOVED_SHAPE_OTHER))
            continue;

        const physx::PxPairFlags status(p.status);

        const bool isEnter = status.isSet(physx::PxPairFlag::eNOTIFY_TOUCH_FOUND);
        const bool isExit = status.isSet(physx::PxPairFlag::eNOTIFY_TOUCH_LOST);

        if (!isEnter && !isExit)
            continue;

        // Enter/Exit 둘 다 올 수 있으니, 둘 다 기록하려면 2번 push하는 게 안전
        if (isEnter)
        {
            m_triggers.push_back(TriggerEvent{ p.triggerActor, p.otherActor, p.triggerShape, p.otherShape, true });
        }
        if (isExit)
        {
            m_triggers.push_back(TriggerEvent{ p.triggerActor, p.otherActor, p.triggerShape, p.otherShape, false });
        }
    }
}

void MMMEngine::PhysXSimulationCallback::DrainContacts(std::vector<ContactEvent>& out)
{
    std::vector<ContactEvent> tmp;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        tmp.swap(m_contacts);
    }
    // 락 밖에서 out에 합치기
    out.insert(out.end(), tmp.begin(), tmp.end());

    //out.insert(out.end(), m_contacts.begin(), m_contacts.end());
    //m_contacts.clear();
}

void MMMEngine::PhysXSimulationCallback::DrainTriggers(std::vector<TriggerEvent>& out)
{
    std::vector<TriggerEvent> tmp;
    {
        std::lock_guard<std::mutex> lock(m_mtx);
        tmp.swap(m_triggers);
    }
    out.insert(out.end(), tmp.begin(), tmp.end());

    //out.insert(out.end(), m_triggers.begin(), m_triggers.end());
    //m_triggers.clear();
}


void MMMEngine::PhysXSimulationCallback::Clear()
{
    std::lock_guard<std::mutex> lock(m_mtx);
    m_contacts.clear();
    m_triggers.clear();
}