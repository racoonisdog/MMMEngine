#include "GameObject.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"
#include "Component.h"


RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<GameObject>("GameObject");


	registration::class_<ObjectPtr<GameObject>>("ObjectPtr<GameObject>")
        .constructor(
			[](const std::string& name) {
				return Object::CreatePtr<GameObject>(name);
			}, registration::protected_access)
		.constructor<>(
			[]() {
				return Object::CreatePtr<GameObject>();
			}, registration::protected_access);
}

MMMEngine::GameObject::GameObject(std::string name)
{
	SetName(name);
}
