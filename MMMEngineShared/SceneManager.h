#pragma once
#include <vector>
#include <memory>
#include <unordered_map>
#include <string>
#include "ExportSingleton.hpp"
#include "GameObject.h"
#include "Scene.h"
#include "SceneRef.h"
#include "Delegates.hpp"

namespace MMMEngine
{
	class MMMENGINE_API SceneManager : public Utility::ExportSingleton<SceneManager>
	{
	private:
		std::unordered_map<std::string, size_t> m_sceneNameToID;			// <Name , ID>
		std::vector<std::unique_ptr<Scene>> m_scenes;

		std::wstring m_sceneListPath;

		size_t m_currentSceneID;
		size_t m_nextSceneID;

		std::unique_ptr<Scene> m_dontDestroyOnLoadScene;

		// todo : json 메세지팩으로 to index, scene snapshot을 Scene생성하면서 로드시키기
		void LoadScenes(bool allowEmptyScene);
		void CreateEmptyScene(std::string name = "EmptyScene");

		void UpdateScenesHash(std::unordered_map<std::string, size_t>&& nameToID) noexcept;
	public:
	
		//========= 메타 프로그램용 =============//
		void RegisterGameObjectToDDOL(ObjPtr<GameObject> go);

		const std::unordered_map<std::string, size_t>& GetScenesHash();

		void ReloadSnapShotCurrentScene(); // 현재 씬의 스냅샷을 갱신 (하드디스크 저장 X, 온 메모리 체인지)
		
		void RebulidAndApplySceneList(std::vector<std::string> sceneList);

		std::vector<ObjPtr<GameObject>> GetAllGameObjectInCurrentScene();
		std::vector<ObjPtr<GameObject>> GetAllGameObjectInDDOL();
		SceneRef GetSceneRef(const Scene* pScene);
		std::vector<Scene*> GetAllSceneToRaw();
		Scene* GetCurrentSceneRaw() { return m_scenes[m_currentSceneID].get(); }
		//=====================================//

		Scene* GetSceneRaw(const SceneRef& ref);
		const SceneRef GetCurrentScene() const;

		Utility::Event<SceneManager, void(void)> onSceneInitBefore{ this };

		const std::wstring GetSceneListPath() const;

		void ChangeScene(const std::string& name);
		void ChangeScene(const size_t& id);

		void StartUp(std::wstring sceneListPath, size_t startSceneIDX, bool allowEmptyScene = false);

		void ShutDown();
		bool CheckSceneIsChanged();

		ObjPtr<GameObject> FindWithMUID(const SceneRef& ref, Utility::MUID muid);

		ObjPtr<GameObject> FindFromAllScenes(const std::string& name);
		ObjPtr<GameObject> FindWithTagFromAllScenes(const std::string& tag);
		std::vector<ObjPtr<GameObject>> FindGameObjectsWithTagFromAllScenes(const std::string& tag);
	};
}