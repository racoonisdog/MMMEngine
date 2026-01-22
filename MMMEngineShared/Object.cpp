#include "GameObject.h"
#include "Object.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"
#include "ObjectManager.h"
#include <stdexcept>
#include <iostream>
#include "Transform.h"
#include "SceneManager.h"

using namespace MMMEngine::Utility;

uint64_t MMMEngine::Object::s_nextInstanceID = 1;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Object>("Object")
		.property("Name", &Object::GetName, &Object::SetName)(rttr::metadata("INSPECTOR", "HIDDEN"))
		.property("MUID", &Object::GetMUID, &Object::SetMUID)
		.property_readonly("InstanceID", &Object::GetInstanceID)
		.property_readonly("isDestroyed", &Object::IsDestroyed)(rttr::metadata("INSPECTOR", "HIDDEN"));

	registration::class_<ObjPtrBase>("ObjPtr")
		.method("IsValid", &ObjPtrBase::IsValid)
		.method("GetRaw", &ObjPtrBase::GetRaw, registration::private_access)
		.method("GetPtrID", &ObjPtrBase::GetPtrID)
		.method("GetPtrGeneration", &ObjPtrBase::GetPtrGeneration);

	registration::class_<ObjPtr<Object>>("ObjPtr<Object>")
		.constructor<>(
			[]() { 
				return Object::NewObject<Object>(); 
			});
}

MMMEngine::Object::Object() : m_instanceID(s_nextInstanceID++)
{
	if (!ObjectManager::Get().IsCreatingObject())
	{
		throw std::runtime_error(
			"Object는 CreatePtr로만 생성할 수 있습니다.\n"
			"스택 생성이나 직접 new 사용이 감지되었습니다."
		);
	}

	m_name = "<Unnamed> [ Instance ID : " + std::to_string(m_instanceID) + " ]";
	m_muid = MUID::NewMUID();
	m_ptrID = UINT32_MAX;
	m_ptrGen = 0;
}

MMMEngine::Object::~Object()
{
	if (!ObjectManager::Get().IsDestroyingObject())
	{
#ifdef _DEBUG
		// Debug: 디버거 중단 + 스택 트레이스
		std::cerr << "\n=== 오브젝트 파괴 오류 ===" << std::endl;
		std::cerr << "Object는 Destroy로만 파괴할 수 있습니다." << std::endl;
		std::cerr << "직접 delete 사용이 감지되었습니다." << std::endl;
		std::cerr << "\n>>> 호출 스택 확인 <<<\n" << std::endl;
		__debugbreak();
#endif
		// Release와 Debug 모두: 즉시 종료
		std::cerr << "\nFATAL ERROR: 허용되지 않는 방법으로 오브젝트 파괴" << std::endl;
		std::abort(); 
	}
}

void MMMEngine::Object::DontDestroyOnLoad(const ObjPtrBase& objPtr)
{
	// GameObject인 경우 그 자체를 씬에게 넘기기
	if (auto go = ObjectManager::Get().GetPtr<Object>(objPtr.GetPtrID(), objPtr.GetPtrGeneration()).Cast<GameObject>())
	{
		// 이미 파괴되었거나 이미 DontDestroyOnLoad 씬에 있으면 처리하지 않음
		if (go->IsDestroyed() || go->GetScene().id_DDOL)
			return;

		// 부모가 있는 경우 부모를 끊기
		go->GetTransform()->SetParent(nullptr);

		std::vector<ObjPtr<GameObject>> gameObjectsToProcess;
		gameObjectsToProcess.push_back(go);

		// BFS (너비 우선 탐색) 방식으로 계층 구조를 순회하여 스택 오버플로우 방지
		while (!gameObjectsToProcess.empty())
		{
			ObjPtr<GameObject> currentGo = gameObjectsToProcess.back();
			gameObjectsToProcess.pop_back();

			// 이미 처리했거나 파괴되었거나 DontDestroyOnLoad 씬에 있으면 건너뜀
			if (currentGo->IsDestroyed() || currentGo->GetScene().id_DDOL)
				continue;

			// 자신을 현재 씬에서 해제하고 DontDestroyOnLoad 씬에 등록
			if (auto sceneRaw = SceneManager::Get().GetSceneRaw(currentGo->GetScene())) // 씬이 유효한지 확인
			{
				sceneRaw->UnRegisterGameObject(currentGo);
			}
			SceneManager::Get().RegisterGameObjectToDDOL(currentGo);

			// 자식들을 큐에 추가하여 다음 반복에서 처리
			for (size_t i = 0; i < currentGo->GetTransform()->GetChildCount(); ++i)
			{
				if (auto childGo = currentGo->GetTransform()->GetChild(i)->GetGameObject())
				{
					gameObjectsToProcess.push_back(childGo);
				}
			}
		}
	}
}

void MMMEngine::Object::Destroy(const ObjPtrBase& objPtr, float delay)
{
	if (ObjectManager::Get().GetPtr<Object>(objPtr.GetPtrID(), objPtr.GetPtrGeneration()).Cast<Transform>())
	{
#ifdef _DEBUG
		assert(false && "Transform은 파괴할 수 없습니다.");
#endif
		return;
	}

	ObjectManager::Get().Destroy(objPtr, delay);
}