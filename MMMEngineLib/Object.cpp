#include "Object.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"
#include "ObjectManager.h"
#include <cassert>

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

MMMEngine::Object::Object() : m_instanceID(s_nextInstanceID++)
{
#ifdef _DEBUG
	if (!ObjectManager::Get()->IsCreatingObject())
	{
		assert(false && "Object는 ObjectManager/CreateInstance로만 생성할 수 있습니다.");
		std::abort();
	}
#endif
	m_name = "<Unnamed> [ Instance ID : " + std::to_string(m_instanceID) + " ]";
	m_guid = GUID::NewGuid();
}

MMMEngine::Object::~Object()
{
#ifdef _DEBUG
	assert(ObjectManager::Get()->IsDestroyingObject() && "Object는 ObjectManager/Destroy로만 파괴할 수 있습니다.");
#endif
}

void MMMEngine::Object::Destroy(MMMEngine::ObjectPtr<MMMEngine::Object> objPtr)
{
	ObjectManager::Get()->Destroy(objPtr);
}
