#include "MissingScriptBehaviour.h"

#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<MissingScriptBehaviour>("MissingScriptBehaviour")
		(rttr::metadata("INSPECTOR", "DONT_ADD_COMP"));
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<MissingScriptBehaviour>>()));

	registration::class_<ObjPtr<MissingScriptBehaviour>>("ObjPtr<MissingScriptBehaviour>")
		.constructor<>(
			[]() {
				return Object::NewObject<MissingScriptBehaviour>();
			});


	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<MissingScriptBehaviour>>();
}
