#pragma once
#include <physx/PxPhysicsAPI.h>

namespace MMMEngine {
	physx::PxFilterFlags CustomFilterShader(
		physx::PxFilterObjectAttributes attributes0,
		physx::PxFilterData filterData0,
		physx::PxFilterObjectAttributes attributes1,
		physx::PxFilterData filterData1,
		physx::PxPairFlags& pairFlags,
		const void* constantBlock,
		physx::PxU32 constantBlockSize
	);
}