#include "Scene.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "SceneSerializer.h"
#include "rttr/registration"
#include "rttr/detail/policies/ctor_policies.h"

RTTR_REGISTRATION
{
    using namespace rttr;
    using namespace MMMEngine;

    registration::class_<Scene>("Scene")
        .method("SetSnapShot", &Scene::SetSnapShot, registration::private_access)
        .property("MUID", &Scene::GetMUID, &Scene::SetMUID, registration::private_access);
}

void MMMEngine::Scene::SetMUID(const Utility::MUID& muid)
{
    m_muid = muid;
}


void MMMEngine::Scene::SetSnapShot(SnapShot&& snapshot) noexcept
{
    m_snapshot = std::move(snapshot);
}

const MMMEngine::SnapShot& MMMEngine::Scene::GetSnapShot() const
{
    return m_snapshot;
}

void MMMEngine::Scene::Clear()
{
    for (auto& go : m_gameObjects)
    {
        if(go.IsValid())
            go->SetScene({ static_cast<size_t>(-1),false });
        Object::Destroy(go);
    }
    m_gameObjects.clear();
}

std::vector<MMMEngine::ObjPtr<MMMEngine::GameObject>> MMMEngine::Scene::GetGameObjects()
{
    return m_gameObjects;
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::Scene::CreateGameObject(std::string name)
{
    return Object::NewObject<GameObject>(SceneManager::Get().GetSceneRef(this), name);
}

void MMMEngine::Scene::Initialize()
{
    SceneSerializer::Get().Deserialize(*this, m_snapshot);
}


MMMEngine::Scene::~Scene()
{
    Clear();
}

void MMMEngine::Scene::RegisterGameObject(ObjPtr<GameObject> go)
{
    m_gameObjects.push_back(go);
}

void MMMEngine::Scene::UnRegisterGameObject(ObjPtr<GameObject> go)
{
    auto it = std::find(m_gameObjects.begin(), m_gameObjects.end(), go);
    if (it != m_gameObjects.end()) {
        *it = std::move(m_gameObjects.back()); // ¸¶Áö¸· ¿ø¼Ò¸¦ µ¤¾î¾¸
        m_gameObjects.pop_back();
    }
}

const std::string& MMMEngine::Scene::GetName() const
{
    return m_name;
}

const MMMEngine::Utility::MUID & MMMEngine::Scene::GetMUID() const
{
    return m_muid;
}

void MMMEngine::Scene::SetName(const std::string& name)
{
    m_name = name;
}
