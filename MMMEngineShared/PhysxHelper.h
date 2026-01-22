#pragma once
#include "SimpleMath.h"
#include <physx/PxPhysicsAPI.h>
#include <physx/foundation/PxTransform.h>

using namespace DirectX::SimpleMath;

template <typename T>
void SAFE_RELEASE(T* p)
{
	if (p)
	{
		p->release();
		p = nullptr;
	}
}

void ConvertVecPhysXtoDirectX(const physx::PxVec3& physx_vec, DirectX::SimpleMath::Vector3& direct_vec);

void ConvertVecDirectXtoPhysX(const DirectX::SimpleMath::Vector3& direct_vec, physx::PxVec3& physx_vec);

void ConvertRotationQuatPhysXtoDirectX(const physx::PxQuat& physx_Quat, DirectX::SimpleMath::Quaternion& direct_Quat);

void ConvertRotationQuatDirectXtoPhysX(const DirectX::SimpleMath::Quaternion& direct_Quat, physx::PxQuat& physx_Quat);

physx::PxVec3 ToPxVec(const float v[3]);

physx::PxVec3 ToPxVec(const Vector3& v);

physx::PxQuat ToPxQuat(const Quaternion& quater);

physx::PxTransform ToPxTrans(const Vector3& pos, const Quaternion& quater);

Vector3 ToVec(const physx::PxVec3& v);

Quaternion ToQuat(const physx::PxQuat& quater);

template<typename T>
bool Contains(const std::vector<T*>& v, T* target)
{
	return std::find(v.begin(), v.end(), target) != v.end();
}

template<typename T>
void EraseOne(std::vector<T*>& v, T* target)
{
	auto it = std::find(v.begin(), v.end(), target);
	if (it != v.end()) v.erase(it);
}