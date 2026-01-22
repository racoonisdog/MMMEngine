#include "PhysicsFilter.h"

physx::PxFilterFlags MMMEngine::CustomFilterShader(physx::PxFilterObjectAttributes attributes0, physx::PxFilterData filterData0, physx::PxFilterObjectAttributes attributes1, physx::PxFilterData filterData1, physx::PxPairFlags& pairFlags, const void* constantBlock, physx::PxU32 constantBlockSize)
{

    PX_UNUSED(constantBlock);
    PX_UNUSED(constantBlockSize);

    //레이어/마스크 검사 (양방향 허용이어야 통과)
    const bool allow =
        (filterData0.word0 & filterData1.word1) != 0 &&
        (filterData1.word0 & filterData0.word1) != 0;

    if (!allow)
        return physx::PxFilterFlag::eSUPPRESS;

    //트리거 / 컨택 분기
    const bool isTrigger =
        physx::PxFilterObjectIsTrigger(attributes0) ||
        physx::PxFilterObjectIsTrigger(attributes1);

    if (isTrigger)
    {
        pairFlags = physx::PxPairFlag::eTRIGGER_DEFAULT;
        pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
        pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_LOST;
        return physx::PxFilterFlag::eDEFAULT;
    }

    pairFlags = physx::PxPairFlag::eCONTACT_DEFAULT;
    pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_FOUND;
    pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_LOST;
    pairFlags |= physx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS; // 콜백이 원하면

    return physx::PxFilterFlag::eDEFAULT;
}
