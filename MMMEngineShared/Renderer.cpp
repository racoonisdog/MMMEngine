#include "Renderer.h"
#include <rttr/registration>
#include "GameObject.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Renderer>("Renderer")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<Renderer>>()))
		.property("isEnabled", &Renderer::GetEnabled, &Renderer::SetEnabled);


	registration::class_<ObjPtr<Renderer>>("ObjPtr<Renderer>")
		.constructor<>(
			[]() {
				return Object::NewObject<Renderer>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<Renderer>>();
}

bool MMMEngine::Renderer::IsActiveAndEnabled()
{
	return isEnabled && GetGameObject().IsValid() && GetGameObject()->IsActiveInHierarchy();
}