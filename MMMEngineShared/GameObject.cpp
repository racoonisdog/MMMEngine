#include "GameObject.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"
#include "Component.h"
#include "Transform.h"
#include "ObjectManager.h"
#include "SceneManager.h"
#include <cmath>

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<GameObject>("GameObject")
		.property("Scene", &GameObject::GetScene, &GameObject::SetScene, registration::private_access)
		.property("Layer", &GameObject::GetLayer, &GameObject::SetLayer)
		.property("Tag", &GameObject::GetTag, &GameObject::SetTag)
		.property_readonly("Components", &GameObject::GetAllComponents);

	registration::class_<ObjPtr<GameObject>>("ObjPtr<GameObject>")
		.constructor(
   			[](const std::string& name) {
   				return Object::NewObject<GameObject>(name);
   			})
   	    .constructor<>(
   		    []() {
   			    return Object::NewObject<GameObject>();
   		    })
		.constructor(
			[](SceneRef scene) {
				return Object::NewObject<GameObject>(scene);
			})
		.constructor(
			[](SceneRef scene, const std::string& name) {
				return Object::NewObject<GameObject>(scene, name);
			});
}

void MMMEngine::GameObject::RegisterComponent(const ObjPtr<Component>& comp)
{
    m_components.push_back(comp);
}

void MMMEngine::GameObject::UnRegisterComponent(const ObjPtr<Component>& comp)
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
	m_transform = NewObject<Transform>();
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
}


MMMEngine::GameObject::GameObject(std::string name) 
{
	SetName(name);
}

MMMEngine::GameObject::GameObject(SceneRef scene)
{
	SetScene(scene);
}

MMMEngine::GameObject::GameObject(SceneRef scene, std::string name)
{
	SetScene(scene);
	SetName(name);
}

void MMMEngine::GameObject::Construct()
{
	auto& sceneManager = SceneManager::Get();
	if (!sceneManager.GetSceneRaw(GetScene()))
	{
		SetScene(sceneManager.GetCurrentScene());
		sceneManager.GetSceneRaw(GetScene())->RegisterGameObject(SelfPtr(this));
	}
	Initialize();
}

void MMMEngine::GameObject::Dispose()
{
	if (auto scene = SceneManager::Get().GetSceneRaw(GetScene()))
	{
		scene->UnRegisterGameObject(SelfPtr(this));
	}

	ObjPtr<Component> t = m_transform;
	t->SetGameObject(nullptr);
	UnRegisterComponent(m_transform);
	ObjectManager::Get().Destroy(m_transform);
	m_transform = nullptr;
	for (const auto& comp : m_components)
		if (comp.IsValid() && !comp->IsDestroyed())
		{
			comp->SetGameObject(nullptr);
			Destroy(comp);
		}

	m_components.clear();
}

void MMMEngine::GameObject::SetActive(bool active)
{
	m_active = active;

	UpdateActiveInHierarchy();
}


// todo : Scene 매니저에서 모든 씬을 순회하면서 찾기 -> 우리가 원하는 로직임
MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::GameObject::Find(const std::wstring& name)
{
	return ObjPtr<GameObject>();
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::GameObject::FindWithTag(const std::string& name)
{
	return ObjPtr<GameObject>();
}

std::vector<MMMEngine::ObjPtr<MMMEngine::GameObject>> MMMEngine::GameObject::FindGameObjectsWithTag(const std::string& name)
{
	return std::vector<ObjPtr<GameObject>>();
}
