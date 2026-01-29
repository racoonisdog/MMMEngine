#include "SceneManager.h"
#include "SceneSerializer.h"
#include "StringHelper.h"

#include "json/json.hpp"
#include <fstream>
#include <filesystem>
#include <iostream>

#include "Camera.h"

DEFINE_SINGLETON(MMMEngine::SceneManager)

void MMMEngine::SceneManager::LoadScenes(bool allowEmptyScene)
{
	//rootPath에서 bin을 읽어서 m_sceneNameToID; // <Name , ID>를 초기화
	auto sceneListfilePath = m_sceneListPath + L"/sceneList.bin";
	std::ifstream file(sceneListfilePath, std::ios::binary);
	if (!file.is_open())
	{
		std::wcerr << L"[SceneManager] Failed to open: " << sceneListfilePath << L"\n";
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
		auto sceneRootPath = m_sceneListPath + L"/" + Utility::StringHelper::StringToWString(filepath);
		std::ifstream sceneFile(sceneRootPath, std::ios::binary);
		if (!sceneFile.is_open())
		{
			if (allowEmptyScene)
			{
				CreateEmptyScene(sceneName);
				auto emptyScene = std::move(m_scenes.back());
				m_scenes.pop_back();
				m_scenes[index] = std::move(emptyScene);
				m_sceneNameToID[sceneName] = index;
				std::cout << u8"씬 리스트에 등록된 씬파일을 찾지 못했습니다. -> Scene File : " << sceneName << u8".scene \n임시 씬을 생성합니다." << std::endl;
			}
			else
			{
				assert(false && ("씬 파일 없음: " + sceneName).c_str());
			}
			continue;
		}

		std::vector<uint8_t> sceneBuffer((std::istreambuf_iterator<char>(sceneFile)),
			std::istreambuf_iterator<char>());
		sceneFile.close();

		nlohmann::json snapshot = nlohmann::json::from_msgpack(sceneBuffer);

		// index 위치에 Scene 배치
		m_scenes[index] = std::make_unique<Scene>();
		m_scenes[index]->m_snapshot = snapshot;
		m_scenes[index]->SetName(sceneName);
	}
}

void MMMEngine::SceneManager::CreateEmptyScene(std::string name)
{
	m_scenes.push_back(std::make_unique<Scene>());
	m_scenes.back()->SetName(name);
	SnapShot snapShot; 
	SceneSerializer::Get().SerializeToMemory(*m_scenes.back(), snapShot, true); //카메라 까지 같이 생성하도록 주입
	m_scenes.back()->SetSnapShot(std::move(snapShot));
}


std::vector<MMMEngine::Scene*> MMMEngine::SceneManager::GetAllSceneToRaw()
{
	std::vector<Scene*> rawScenes;
	for (auto& scene : m_scenes)
		rawScenes.push_back(scene.get());

	return rawScenes;
}

const MMMEngine::SceneRef MMMEngine::SceneManager::GetCurrentScene() const
{
	return { m_currentSceneID , false };
}

const std::wstring MMMEngine::SceneManager::GetSceneListPath() const
{
	return m_sceneListPath;
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

const std::unordered_map<std::string, size_t>& MMMEngine::SceneManager::GetScenesHash()
{
	return m_sceneNameToID;
}

void MMMEngine::SceneManager::ReloadSnapShotCurrentScene()
{
	SnapShot snapshot;
	auto currentScene = m_scenes[m_currentSceneID].get();
	SceneSerializer::Get().SerializeToMemory(*currentScene, snapshot);
	currentScene->SetSnapShot(std::move(snapshot));
}

void MMMEngine::SceneManager::RebulidAndApplySceneList(std::vector<std::string> sceneList)
{
	// 현재 씬 배열에 존재하는지 체크, 새로운 것도 체크
	std::string currentSceneName = m_scenes[m_currentSceneID]->GetName();

	std::unordered_map<std::string, size_t> changedNameToID;
	std::vector<std::unique_ptr<Scene>> changedScenes;
	size_t currentIDX = 0;
	for (const auto& sceneName : sceneList)
	{
		changedNameToID[sceneName] = currentIDX++;

		int idx = -1;
		if (m_sceneNameToID.find(sceneName) != m_sceneNameToID.end())
		{
			idx = m_sceneNameToID[sceneName];
		}
		
		// 씬 리스트에 있는 씬
		if (idx != -1)
		{
			changedScenes.push_back(std::move(m_scenes[idx]));
		}
		// 기존 씬 리스트에 없는 신규 씬
		else
		{
			// 파일 경로 탐색
			// Scene 파일 로드
			auto sceneRootPath = m_sceneListPath + L"/" + Utility::StringHelper::StringToWString(sceneName) + L".scene";
			std::ifstream sceneFile(sceneRootPath, std::ios::binary);
		
			if (sceneFile.is_open())
			{
				std::vector<uint8_t> sceneBuffer((std::istreambuf_iterator<char>(sceneFile)),
					std::istreambuf_iterator<char>());
				nlohmann::json snapshot = nlohmann::json::from_msgpack(sceneBuffer);
				sceneFile.close();
				changedScenes.push_back(std::move(std::make_unique<Scene>()));
				changedScenes.back()->SetSnapShot(std::move(snapshot));
				changedScenes.back()->SetName(sceneName);
			}
			else
			{
				// 씬 배열에 빈 씬 추가
				CreateEmptyScene(sceneName);
				// 낚아 채기
				changedScenes.push_back(std::move(m_scenes.back()));
				m_scenes.pop_back();
			}
		}
	}

	// 해쉬 변경
	UpdateScenesHash(std::move(changedNameToID));
	m_scenes = std::move(changedScenes);

	if (!currentSceneName.empty() &&
		m_sceneNameToID.find(currentSceneName) != m_sceneNameToID.end())
	{
		m_currentSceneID = m_sceneNameToID[currentSceneName];

		// GameObject의 Scene 참조 업데이트
		auto gos = m_scenes[m_currentSceneID]->GetGameObjects();
		for (auto& go : gos)
		{
			if (go.IsValid())
				go->SetScene({ m_currentSceneID, false });
		}
	}
	else
	{
		std::cout << u8"경고! : 열려있던 씬의 정보가 씬 리스트에서 사라졌습니다. "
			<< u8"0번째 씬을 로드합니다." << std::endl;
		m_currentSceneID = 0;
		m_nextSceneID = 0;
	}
}

void MMMEngine::SceneManager::ClearDDOLScene()
{
	m_dontDestroyOnLoadScene->Clear();
}

void MMMEngine::SceneManager::UpdateScenesHash(std::unordered_map<std::string, size_t>&& nameToID) noexcept
{
	m_sceneNameToID = std::move(nameToID);
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

void MMMEngine::SceneManager::StartUp(std::wstring sceneListPath, size_t startSceneIDX, bool allowEmptyScene)
{
	m_sceneListPath = sceneListPath;
	m_dontDestroyOnLoadScene = std::make_unique<Scene>();

	// 주어진 경로로 씬리스트 바이너리를 읽고 씬파일경로를 불러와 ID맵을 초기화하고 초기씬을 생성함
	LoadScenes(allowEmptyScene);

	if (m_scenes.empty())
	{
		if(!allowEmptyScene)
			assert(false && "씬리스트가 비어있습니다!, 초기씬 로드에 실패했습니다!");
		else
		{
			CreateEmptyScene();
			m_sceneNameToID[m_scenes.back()->GetName()] = 0;
			m_nextSceneID = 0;
			return;
		}
	}

	m_nextSceneID = startSceneIDX;
}


void MMMEngine::SceneManager::ShutDown()
{
	m_dontDestroyOnLoadScene.reset();
	m_scenes.clear();
	m_currentSceneID = static_cast<size_t>(-1);
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

		onSceneInitBefore(this);
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

std::vector<MMMEngine::ObjPtr< MMMEngine::GameObject>> MMMEngine::SceneManager::GetAllGameObjectInDDOL()
{
	if (m_dontDestroyOnLoadScene)
		return m_dontDestroyOnLoadScene->GetGameObjects();
	else
		std::vector<ObjPtr<GameObject>>();
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
	if (m_dontDestroyOnLoadScene.get())
	{
		const auto& ddol_gameobjs_cache = m_dontDestroyOnLoadScene->GetGameObjects();
		for (auto& ddol_go : ddol_gameobjs_cache)
		{
			if (ddol_go->GetTag() == tag)
			{
				cache.push_back(ddol_go);
			}
		}
	}
	return cache;
}
