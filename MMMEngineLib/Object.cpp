#include "Object.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"
#include "ObjectManager.h"

uint64_t MMMEngine::Object::s_nextInstanceID = 1;


RTTR_REGISTRATION
{
	using namespace rttr;

	registration::class_<MMMEngine::Object>("Object")
		.property("Name", &MMMEngine::Object::GetName, &MMMEngine::Object::SetName)
		.property("GUID", &MMMEngine::Object::GetGUID, &MMMEngine::Object::SetGUID)
		.property_readonly("InstanceID", &MMMEngine::Object::GetInstanceID)
		.property_readonly("isDestroyed", &MMMEngine::Object::IsDestroyed);
}

template<typename T, typename ...Args>
MMMEngine::ObjectPtr<T> MMMEngine::Object::CreateInstance(Args&& ...args)
{
	return ObjectManager::Get()->CreateHandle<T>(std::forward<Args>(args)...);
}

void MMMEngine::Object::Destroy(MMMEngine::ObjectPtr<MMMEngine::Object> objPtr)
{
	ObjectManager::Get()->Destroy(objPtr);
}