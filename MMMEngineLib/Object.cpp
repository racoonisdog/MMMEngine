#include "Object.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"
#include "ObjectManager.h"
#include <stdexcept>
#include <iostream>

uint64_t MMMEngine::Object::s_nextInstanceID = 1;


RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Object>("Object")
		.property("Name", &Object::GetName, &Object::SetName)
		.property("GUID", &Object::GetGUID, &Object::SetGUID)
		.property_readonly("InstanceID", &Object::GetInstanceID)
		.property_readonly("isDestroyed", &Object::IsDestroyed);

	registration::class_<ObjectPtrBase>("ObjectPtr")
		.method("IsValid", &ObjectPtrBase::IsValid)
		.method("GetBase", &ObjectPtrBase::GetBase, registration::private_access)
		.method("GetPtrID", &ObjectPtrBase::GetPtrID)
		.method("GetPtrGeneration", &ObjectPtrBase::GetPtrGeneration);

	registration::class_<ObjectPtr<Object>>("ObjectPtr<Object>")
		.constructor<>(
			[]() {
				return Object::CreateInstance<Object>();
			}, registration::protected_access);
}

MMMEngine::Object::Object() : m_instanceID(s_nextInstanceID++)
{
	if (!ObjectManager::Get().IsCreatingObject())
	{
		throw std::runtime_error(
			"Object는 ObjectManager/CreateInstance로만 생성할 수 있습니다.\n"
			"스택 생성이나 직접 new 사용이 감지되었습니다."
		);
	}

	m_name = "<Unnamed> [ Instance ID : " + std::to_string(m_instanceID) + " ]";
	m_guid = GUID::NewGuid();
}

MMMEngine::Object::~Object()
{
	if (!ObjectManager::Get().IsDestroyingObject())
	{
#ifdef _DEBUG
		// Debug: 디버거 중단 + 스택 트레이스
		std::cerr << "\n=== 오브젝트 파괴 오류 ===" << std::endl;
		std::cerr << "Object는 ObjectManager/Destroy로만 파괴할 수 있습니다." << std::endl;
		std::cerr << "직접 delete 사용이 감지되었습니다." << std::endl;
		std::cerr << "\n>>> 호출 스택 확인 <<<\n" << std::endl;
		__debugbreak();
#endif
		// Release와 Debug 모두: 즉시 종료
		std::cerr << "\nFATAL ERROR: 허용되지 않는 방법으로 오브젝트 파괴" << std::endl;
		std::abort(); 
	}
}

void MMMEngine::Object::Destroy(const ObjectPtrBase& objPtr, float delay)
{
	ObjectManager::Get().Destroy(objPtr, delay);
}