#include "PhysicsFilter.h"

physx::PxFilterFlags MMMEngine::CustomFilterShader(physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0, physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1, physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize)
{

    //충돌무시
    if ((filterData0.word0 & filterData1.word1) == 0 &&
        (filterData1.word0 & filterData0.word1) == 0)
        return physx::PxFilterFlag::eSUPPRESS;

    //트리거처리
    if (physx::PxFilterObjectIsTrigger(attributes0) || physx::PxFilterObjectIsTrigger(attributes1))
    {
        pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
        pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
        pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_LOST;

        return physx::PxFilterFlag::eDEFAULT;
    }

    //일반 물리 충돌 + 이벤트 통지
    pairFlags =
        physx::PxPairFlag::eSOLVE_CONTACT |
        physx::PxPairFlag::eDETECT_DISCRETE_CONTACT |
        physx::PxPairFlag::eNOTIFY_TOUCH_FOUND |
        physx::PxPairFlag::eNOTIFY_TOUCH_LOST;

    pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;
    pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
    pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_LOST;

    return physx::PxFilterFlag::eDEFAULT;
}
