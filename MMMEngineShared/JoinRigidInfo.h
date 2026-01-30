#pragma once
#include "Component.h"
#include "RigidBodyComponent.h"

// join해제시 rigid 생성할때 m_ColliderMaster true해주기

namespace MMMEngine
{
	class MMMENGINE_API JoinRigidInfo : public Component
	{
	public:
		void Initialize() override;
		void UnInitialize() override;

		void CopyRigidInfo(MMMEngine::ObjPtr<MMMEngine::RigidBodyComponent> ptr_rigid);
		MMMEngine::RigidBodyComponent::Desc GetRigidInfo();

	private:
		MMMEngine::RigidBodyComponent::Desc back_Desc{};
	};
}

