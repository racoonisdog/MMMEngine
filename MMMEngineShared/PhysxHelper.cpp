#include "PhysXHelper.h"

void ConvertVecPhysXtoDirectX(const physx::PxVec3& physx_vec, DirectX::SimpleMath::Vector3& direct_vec)
{
	direct_vec.x = physx_vec.x;
	direct_vec.y = physx_vec.y;
	direct_vec.z = physx_vec.z;
}

void ConvertVecDirectXtoPhysX(const DirectX::SimpleMath::Vector3& direct_vec, physx::PxVec3& physx_vec)
{
	physx_vec.x = direct_vec.x;
	physx_vec.y = direct_vec.y;
	physx_vec.z = direct_vec.z;
}

void ConvertRotationQuatPhysXtoDirectX(const physx::PxQuat& physx_Quat, DirectX::SimpleMath::Quaternion& direct_Quat)
{
	direct_Quat.x = physx_Quat.x;
	direct_Quat.y = physx_Quat.y;
	direct_Quat.z = physx_Quat.z;
	direct_Quat.w = physx_Quat.w;
}

void ConvertRotationQuatDirectXtoPhysX(const DirectX::SimpleMath::Quaternion& direct_Quat, physx::PxQuat& physx_Quat)
{
	physx_Quat.x = direct_Quat.x;
	physx_Quat.y = direct_Quat.y;
	physx_Quat.z = direct_Quat.z;
	physx_Quat.w = direct_Quat.w;
}

physx::PxVec3 ToPxVec(const float v[3])
{
	return physx::PxVec3(v[0], v[1], v[2]);
}

physx::PxVec3 ToPxVec(const Vector3& v)
{
	return physx::PxVec3(v.x, v.y, v.z);
}

physx::PxQuat ToPxQuat(const Quaternion& quater)
{
	return physx::PxQuat(quater.x, quater.y, quater.z, quater.w);
}

physx::PxTransform ToPxTrans(const Vector3& pos, const Quaternion& quater)
{
	return physx::PxTransform(
		ToPxVec(pos), ToPxQuat(quater)
	);
}

Vector3 ToVec(const physx::PxVec3& v)
{
	return Vector3(v.x, v.y, v.z);
}

Quaternion ToQuat(const physx::PxQuat& quater)
{
	return Quaternion(quater.x, quater.y, quater.z, quater.w);
}


