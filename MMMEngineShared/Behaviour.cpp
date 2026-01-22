#include "Behaviour.h"
#include "BehaviourManager.h"

#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Behaviour>("Behaviour")
		(rttr::metadata("INSPECTOR", "DONT_ADD_COMP"))
		.property("Enabled", &Behaviour::GetEnabled, &Behaviour::SetEnabled)
		.property_readonly("IsActiveAndEnabled", &Behaviour::IsActiveAndEnabled)(rttr::metadata("INSPECTOR", "HIDDEN"));

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<Behaviour>>();
}


MMMEngine::Behaviour::Behaviour() 
	: m_enabled(true)
{
	
}

void MMMEngine::Behaviour::Initialize()
{
	BehaviourManager::Get().RegisterBehaviour(SelfPtr(this));
}

void MMMEngine::Behaviour::UnInitialize()
{
	BehaviourManager::Get().UnRegisterBehaviour(SelfPtr(this)); // BehaviourManager에서 제거
	m_messages.clear();
}

void MMMEngine::Behaviour::SetEnabled(bool value)
{
	if (value != m_enabled)
	{
		//바뀔 때 무언갈 실행하는 코드 작성하기
		m_enabled = value;
	}
}

bool MMMEngine::Behaviour::IsActiveAndEnabled()
{
	return m_enabled && GetGameObject().IsValid() && GetGameObject()->IsActiveInHierarchy();
}
