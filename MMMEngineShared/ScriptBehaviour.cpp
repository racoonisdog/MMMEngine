#include "ScriptBehaviour.h"

#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<ScriptBehaviour>("ScriptBehaviour")
		(rttr::metadata("INSPECTOR", "DONT_ADD_COMP"));

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<ScriptBehaviour>>();
}
