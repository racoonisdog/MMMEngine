#include "SceneManager.h"
#include "SceneSerializer.h"
#include "StringHelper.h"

#include "json/json.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>

DEFINE_SINGLETON(MMMEngine::SceneManager)

void MMMEngine::SceneManager::LoadScenes(std::wstring rootPath)
{

	//rootPath에서 bin을 읽어서 m_sceneNameToID; // <Name , ID>를 초기화
	auto sceneListPath = rootPath + L"/sceneList.bin";
	std::ifstream file(sceneListPath, std::ios::binary);
	if (!file.is_open())
	{
		std::wcerr << L"[SceneManager] Failed to open: " << sceneListPath << L"\n";
		std::wcerr << L"[SceneManager] CWD: " << std::filesystem::current_path().wstring() << L"\n";
		return;
	}
	std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	file.close();
	nlohmann::json sceneList = nlohmann::json::from_msgpack(buffer);

	m_sceneNameToID.clear();
	m_scenes.clear();

	// index 기준으로 정렬
	std::vector<std::pair<size_t, std::string>> sortedScenes;
	for (const auto& sceneEntry : sceneList)
	{
		std::string filepath = sceneEntry["filepath"];
		size_t index = sceneEntry["index"];
		sortedScenes.emplace_back(index, filepath);
	}
	std::sort(sortedScenes.begin(), sortedScenes.end());

	// 정렬된 순서대로 m_scenes 크기 미리 할당
	m_scenes.resize(sortedScenes.size());

	for (const auto& [index, filepath] : sortedScenes)
	{
		// .scene 확장자 제거하여 순수 이름만 추출
		std::string sceneName = filepath.substr(0, filepath.find(".scene"));
		m_sceneNameToID[sceneName] = index;

		// Scene 파일 로드
		auto sceneRootPath = rootPath + L"/" + Utility::StringHelper::StringToWString(filepath);
		std::ifstream sceneFile(sceneRootPath, std::ios::binary);
		if (!sceneFile.is_open())
		{
			// todo : 에러로그 만들기, 지금은 빈 씬만 생성
			m_scenes[index] = std::make_unique<Scene>();
			m_sceneNameToID[sceneName] = index;
			continue;
		}

		std::vector<uint8_t> sceneBuffer((std::istreambuf_iterator<char>(sceneFile)),
			std::istreambuf_iterator<char>());
		sceneFile.close();

		nlohmann::json snapshot = nlohmann::json::from_msgpack(sceneBuffer);

		// index 위치에 Scene 배치
		m_scenes[index] = std::make_unique<Scene>();
		m_scenes[index]->m_snapshot = snapshot;
	}
}

void MMMEngine::SceneManager::CreateEmptyScene()
{
	m_scenes.push_back(std::make_unique<Scene>());
	m_scenes.back()->SetName("EmptyScene");
	SnapShot snapShot; 
	SceneSerializer::Get().SerializeToMemory(*m_scenes.back(), snapShot);
	m_scenes.back()->SetSnapShot(std::move(snapShot));
	m_currentSceneID = 0;
}


const MMMEngine::SceneRef MMMEngine::SceneManager::GetCurrentScene() const
{
	return { m_currentSceneID , false };
}

void MMMEngine::SceneManager::RegisterGameObjectToDDOL(ObjPtr<GameObject> go)
{
	if (m_dontDestroyOnLoadScene)
	{
		m_dontDestroyOnLoadScene->RegisterGameObject(go);
		go->SetScene({ static_cast<size_t>(-1),true });
	}
#ifdef _DEBUG
	else
		assert(true && "Dont Destroy On Load 씬이 존재하지 않습니다.");
#endif;
}

MMMEngine::Scene* MMMEngine::SceneManager::GetSceneRaw(const SceneRef& ref)
{
	if (m_scenes.size() <= ref.id && !ref.id_DDOL)
		return nullptr;

	if (ref.id_DDOL)
		return m_dontDestroyOnLoadScene.get();

	return m_scenes[ref.id].get();
}

MMMEngine::SceneRef MMMEngine::SceneManager::GetSceneRef(const Scene* pScene)
{
	size_t idx = 0;
	for (const auto& uniqueP : m_scenes)
	{
		if (uniqueP.get() == pScene)
		{
			return SceneRef{ idx,false };
		}
		++idx;
	}

	return SceneRef{ static_cast<size_t>(-1),false };
}

void MMMEngine::SceneManager::ChangeScene(const std::string& name)
{
	auto it = m_sceneNameToID.find(name);
	if (it != m_sceneNameToID.end())
	{
		m_nextSceneID = it->second;
	}
}

void MMMEngine::SceneManager::ChangeScene(const size_t& id)
{
	if (id < m_scenes.size())
		m_nextSceneID = id;
}

void MMMEngine::SceneManager::StartUp(std::wstring rootPath, bool allowEmptyScene)
{
	m_dontDestroyOnLoadScene = std::make_unique<Scene>();

	// 고정된 경로로 json 바이너리를 읽고 씬파일경로를 불러와 ID맵을 초기화하고 초기씬을 생성함
	LoadScenes(rootPath);

	if (m_scenes.empty())
	{
		if(!allowEmptyScene)
			assert(false && "씬리스트가 비어있습니다!, 초기씬 로드에 실패했습니다!");
		else
			CreateEmptyScene();
	}

	m_nextSceneID = 0;
}


void MMMEngine::SceneManager::ShutDown()
{
	m_dontDestroyOnLoadScene.reset();
	m_scenes.clear();
}

bool MMMEngine::SceneManager::CheckSceneIsChanged()
{
	if (m_nextSceneID != static_cast<size_t>(-1) &&
		m_nextSceneID < m_scenes.size())
	{
		if (m_currentSceneID != static_cast<size_t>(-1) && 
			m_currentSceneID < m_scenes.size())
		 	m_scenes[m_currentSceneID]->Clear();

		m_currentSceneID = m_nextSceneID;
		m_nextSceneID = static_cast<size_t>(-1);

		m_scenes[m_currentSceneID]->Initialize();
		return true;
	}

	return false;
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::SceneManager::FindWithMUID(const SceneRef& ref, Utility::MUID muid)
{
	auto scene = m_scenes[ref.id].get();

	const auto& objs = scene->GetGameObjects();
	for (auto& go : objs)
	{
		if (go->GetMUID() == muid)
			return go;
	}

	return nullptr;
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::SceneManager::FindFromAllScenes(const std::string& name)
{
	for (auto& scene : m_scenes)
	{
		const auto& gameobjs_cache = scene->GetGameObjects();
		for (auto& go : gameobjs_cache)
		{
			if (go->GetName() == name)
			{
				return go;
			}
		}
	}
	const auto& ddol_gameobjs_cache = m_dontDestroyOnLoadScene->GetGameObjects();
	for (auto& ddol_go : ddol_gameobjs_cache)
	{
		if (ddol_go->GetName() == name)
		{
			return ddol_go;
		}
	}

	return nullptr;
}

MMMEngine::ObjPtr< MMMEngine::GameObject> MMMEngine::SceneManager::FindWithTagFromAllScenes(const std::string& tag)
{
	for (auto& scene : m_scenes)
	{
		const auto& gameobjs_cache = scene->GetGameObjects();
		for (auto& go : gameobjs_cache)
		{
			if (go->GetTag() == tag)
			{
				return go;
			}
		}
	}
	const auto& ddol_gameobjs_cache = m_dontDestroyOnLoadScene->GetGameObjects();
	for (auto& ddol_go : ddol_gameobjs_cache)
	{
		if (ddol_go->GetTag() == tag)
		{
			return ddol_go;
		}
	}

	return nullptr;
}

std::vector<MMMEngine::ObjPtr< MMMEngine::GameObject>> MMMEngine::SceneManager::GetAllGameObjectInCurrentScene()
{
	if (m_currentSceneID >= m_scenes.size())
		return std::vector<ObjPtr<GameObject>>();

	return m_scenes[m_currentSceneID]->GetGameObjects();
}

std::vector<MMMEngine::ObjPtr< MMMEngine::GameObject>> MMMEngine::SceneManager::FindGameObjectsWithTagFromAllScenes(const std::string& tag)
{
	std::vector<ObjPtr<GameObject>> cache;

	for (auto& scene : m_scenes)
	{
		const auto& gameobjs_cache = scene->GetGameObjects();
		for (auto& go : gameobjs_cache)
		{
			if (go->GetTag() == tag)
			{
				cache.push_back(go);
			}
		}
	}
	const auto& ddol_gameobjs_cache = m_dontDestroyOnLoadScene->GetGameObjects();
	for (auto& ddol_go : ddol_gameobjs_cache)
	{
		if (ddol_go->GetTag() == tag)
		{
			cache.push_back(ddol_go);
		}
	}

	return cache;
}
