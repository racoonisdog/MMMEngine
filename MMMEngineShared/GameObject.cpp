#include "GameObject.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"
#include "Component.h"
#include "Transform.h"
#include "ObjectManager.h"
#include "SceneManager.h"
#include <cmath>

uint64_t MMMEngine::GameObject::s_go_instanceID = 0;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<GameObject>("GameObject")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<GameObject>>()))
		.property("Active", &GameObject::IsActiveInHierarchy, &GameObject::SetActive)
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

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<GameObject>>();
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
	std::string duplicateNum = "";
	if (s_go_instanceID > 0)
	{
		duplicateNum = "(" + std::to_string(s_go_instanceID) + ")";
	}

	SetName("GameObject" + duplicateNum);
	s_go_instanceID++;
}


MMMEngine::GameObject::GameObject(std::string name) 
{
	SetName(name);
}

MMMEngine::GameObject::GameObject(SceneRef scene)
{
	std::string duplicateNum = "";
	if (s_go_instanceID > 0)
	{
		duplicateNum = "(" + std::to_string(s_go_instanceID) + ")";
	}

	SetName("GameObject" + duplicateNum);
	s_go_instanceID++;
	m_scene = scene;
}

MMMEngine::GameObject::GameObject(SceneRef scene, std::string name)
{
	m_scene = scene;
	SetName(name);
}

void MMMEngine::GameObject::Construct()
{
	auto& sceneManager = SceneManager::Get();
	if (!sceneManager.GetSceneRaw(m_scene))
	{
		m_scene = sceneManager.GetCurrentScene();
		sceneManager.GetSceneRaw(m_scene)->RegisterGameObject(SelfPtr(this));
	}
	Initialize();
}

void MMMEngine::GameObject::Dispose()
{
	if (auto scene = SceneManager::Get().GetSceneRaw(m_scene))
	{
		scene->UnRegisterGameObject(SelfPtr(this));
	}

	for (auto& child : m_transform->m_childs)
	{
		if (child.IsValid() 
			&& !child->IsDestroyed())
		{
			Destroy(child->GetGameObject());
		}
	}
	m_transform->SetParent(nullptr);
	m_transform->SetGameObject(nullptr);
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

MMMEngine::ObjPtr<MMMEngine::Component> MMMEngine::GameObject::AddComponent(rttr::type compType)
{
	if (!compType.is_valid())
		assert(false && "AddComponent : RTTR 타입이 올바르지 않습니다!");

	rttr::type base = rttr::type::get<Component>();

	// compType이 혹시 포인터/래퍼로 들어오더라도 raw로 정규화해서 검사
	rttr::type raw = compType.get_raw_type();

	if (!raw.is_derived_from(base))
		assert(false && "AddComponent : 컴포넌트가 아닙니다!");

	// raw 타입에서 wrapper_type(ObjPtr<Derived>) 얻기
	rttr::variant md = raw.get_metadata("wrapper_type");
	if (!md.is_valid())
		assert(false && "AddComponent : wrapper_type metadata가 없습니다!");

	rttr::type wrapperType = md.get_value<rttr::type>();

	rttr::variant compVariant = wrapperType.create();
	if (!compVariant.is_valid())
		assert(false && "AddComponent : 컴포넌트가 생성되지 않았습니다!");

	// 여기서부터는 항상 ObjPtr<Derived> -> ObjPtr<Component> 변환이 됨
	ObjPtr<Component> comp = compVariant.convert<ObjPtr<Component>>();
	if (!comp.IsValid())
		assert(false && "AddComponent : 변환 실패!");

	comp->m_gameObject = SelfPtr(this);
	RegisterComponent(comp);
	comp->Initialize();
	return comp;
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::GameObject::Find(const std::string& name)
{
	return SceneManager::Get().FindFromAllScenes(name);
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::GameObject::FindWithTag(const std::string& tag)
{
	return SceneManager::Get().FindWithTagFromAllScenes(tag);
}

std::vector<MMMEngine::ObjPtr<MMMEngine::GameObject>> MMMEngine::GameObject::FindGameObjectsWithTag(const std::string& tag)
{
	return SceneManager::Get().FindGameObjectsWithTagFromAllScenes(tag);
}
