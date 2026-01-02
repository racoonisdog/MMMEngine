#include "GameObject.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"


RTTR_REGISTRATION
{
	using namespace rttr;

	registration::class_<MMMEngine::GameObject>("GameObject");
}
