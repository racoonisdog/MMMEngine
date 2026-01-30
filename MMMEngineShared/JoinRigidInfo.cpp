#include "pch.h"
#include "JoinRigidInfo.h"
#include "GameObject.h"

void MMMEngine::JoinRigidInfo::Initialize()
{
	auto it = GetGameObject()->GetComponent<RigidBodyComponent>();
	CopyRigidInfo(it);
}

void MMMEngine::JoinRigidInfo::UnInitialize()
{

}

void MMMEngine::JoinRigidInfo::CopyRigidInfo(MMMEngine::ObjPtr<MMMEngine::RigidBodyComponent> ptr_rigid)
{
	bool useGravitiy = ptr_rigid->GetUseGravity();
	bool isKinematic = ptr_rigid->GetKinematic();
	float mass = ptr_rigid->GetMass();
	float linearDamping = ptr_rigid->GetLineDamping();
	float angularDamping = ptr_rigid->GetAngularDamping();
	MMMEngine::RigidBodyComponent::Type type = ptr_rigid->GetType();
	bool lockPosX = ptr_rigid->GetLockPosX();
	bool lockPosY = ptr_rigid->GetLockPosY();
	bool lockPosZ = ptr_rigid->GetLockPosZ();
	bool lockRotX = ptr_rigid->GetLockRotX();
	bool lockRotY = ptr_rigid->GetLockRotY();
	bool lockRotZ = ptr_rigid->GetLockRotZ();
	uint32_t solverPositionIters = ptr_rigid->GetSolverPositionIters();
	uint32_t solverVelocityIters = ptr_rigid->GetSolverVelocityIters();
	MMMEngine::RigidBodyComponent::CollisionDetectionMode collisionMode = ptr_rigid->GetCollisionMode();
	MMMEngine::RigidBodyComponent::InterpolationMode interpolation = ptr_rigid->GetInterpolationMode();


	back_Desc = MMMEngine::RigidBodyComponent::Desc{ type,  mass , linearDamping , angularDamping , useGravitiy , isKinematic , lockPosX ,
		lockPosY, lockPosZ, lockRotX, lockRotY, lockRotZ, solverPositionIters, solverVelocityIters, collisionMode, interpolation
	};
}

MMMEngine::RigidBodyComponent::Desc MMMEngine::JoinRigidInfo::GetRigidInfo()
{
	return back_Desc;
}
