#include "Renderer.h"
#include <rttr/registration>
#include "GameObject.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Renderer>("Renderer")
		(rttr::metadata("wrapper_type_name", "ObjPtr<Renderer>"))(rttr::metadata("INSPECTOR", "DONT_ADD_COMP"))
		.property("isEnabled", &Renderer::GetEnabled, &Renderer::SetEnabled);


	registration::class_<ObjPtr<Renderer>>("ObjPtr<Renderer>")
		.constructor<>(
			[]() {
				return Object::NewObject<Renderer>();
			})
		.method("Inject", &ObjPtr<Renderer>::Inject);
}

bool MMMEngine::Renderer::IsActiveAndEnabled()
{
	return isEnabled && GetGameObject().IsValid() && GetGameObject()->IsActiveInHierarchy();
}
