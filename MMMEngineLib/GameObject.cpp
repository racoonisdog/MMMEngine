#include "GameObject.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"
#include "Component.h"
#include "Transform.h"
#include "ObjectManager.h"
#include <cmath>

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<GameObject>("GameObject")
		.property("Layer", &GameObject::GetLayer, &GameObject::SetLayer)
		.property("Tag", &GameObject::GetTag, &GameObject::SetTag);

	registration::class_<ObjectPtr<GameObject>>("ObjectPtr<GameObject>")
		.constructor(
   			[](const std::string& name) {
   				return Object::CreatePtr<GameObject>(name);
   			})
   	    .constructor<>(
   		    []() {
   			    return Object::CreatePtr<GameObject>();
   		    });
}

void MMMEngine::GameObject::RegisterComponent(ObjectPtr<Component> comp)
{
    m_components.push_back(comp);
}

void MMMEngine::GameObject::UnRegisterComponent(ObjectPtr<Component> comp)
{
    auto it = std::find(m_components.begin(), m_components.end(), comp);
    if (it != m_components.end()) {
        *it = std::move(m_components.back()); // 마지막 원소를 덮어씀
        m_components.pop_back();
    }
}

void MMMEngine::GameObject::Initialize()
{
	//transform은 직접 생성 후 m_components에 등록
	m_transform = CreatePtr<Transform>();
	m_transform->SetGameObject(SelfPtr(this));
	m_transform->SetParent(nullptr); // 부모가 없으므로 nullptr로 설정

	RegisterComponent(m_transform); // Transform을 컴포넌트로 등록

	UpdateActiveInHierarchy();

	//s_allGameObjects.push_back(this); // 모든 게임 오브젝트 목록에 추가
}

void MMMEngine::GameObject::UpdateActiveInHierarchy()
{
	if (auto parent = m_transform->GetParent())
	{
		m_activeInHierarchy = parent->GetGameObject()->IsActiveInHierarchy() && m_active;
	}
	else
	{
		m_activeInHierarchy = m_active; // 부모가 없으면 자기 자신만 활성화 여부를 따짐
	}

	// 자식들의 활성화 상태 갱신
	for (auto& child : m_transform->m_childs)
	{
		if(child.IsValid())
			child->GetGameObject()->UpdateActiveInHierarchy();
	}
}

MMMEngine::GameObject::GameObject()
{
	//m_scene = SceneManager::Get() ? SCENE_GET_CURRENTSCENE() : nullptr;
	//m_scene->RegisterGameObject(this);
	Initialize();
}


MMMEngine::GameObject::GameObject(std::string name)
{
	SetName(name);
	Initialize();
}

void MMMEngine::GameObject::BeforeDestroy()
{
	ObjectManager::Get().Destroy(m_transform);
}

void MMMEngine::GameObject::SetActive(bool active)
{
	m_active = active;

	UpdateActiveInHierarchy();
}
