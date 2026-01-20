#pragma once
#include <unordered_set>
#include <string>
#include <queue>
#include <algorithm>

#include "ExportSingleton.hpp"
#include "Behaviour.h"
#include "ScriptLoader.h"

namespace MMMEngine
{
	class MMMENGINE_API BehaviourManager : public Utility::ExportSingleton<BehaviourManager>
	{
	private:
		friend class Behaviour;

		bool m_needSort = false; // Behaviour 정렬이 필요한지 여부
		std::vector<ObjPtr<Behaviour>> m_activeBehaviours; // 활성화된 Behaviour를 저장하는 벡터
		std::vector<ObjPtr<Behaviour>> m_inactiveBehaviours; // 비활성화된 Behaviour를 저장하는 벡터
		std::unordered_set<ObjPtr<Behaviour>> m_firstCallBehaviours;
		std::unique_ptr<ScriptLoader> m_pScriptLoader;

		// Behaviour를 등록하는 함수
		void RegisterBehaviour(ObjPtr<Behaviour> behaviour);

		// Behaviour를 제거하는 함수
		void UnRegisterBehaviour(ObjPtr<Behaviour> behaviour);

	public:
		// ExcutionOrder에 따라 Behaviour를 정렬하는 함수
		void SortBehaviours();
		void AllSortBehaviours();

		void InitializeBehaviours();

		// 비활성화된 Behaviour를 감지하는 함수
		void DisableBehaviours();

		void BroadCastBehaviourMessage(const std::string& messageName);

		// 매개변수 있는 브로드캐스트
		template<typename... Args>
		void BroadCastBehaviourMessage(const std::string& messageName, Args&&... args)
		{
			for (auto& behaviour : m_activeBehaviours)
			{
				behaviour->CallMessage(messageName, std::forward<Args>(args)...);
			}
		}

		void ReloadUserScripts(const std::string& name);

		template<typename... Args>
		void AllBroadCastBehaviourMessage(const std::string& messageName, Args&&... args)
		{
			for (auto& behaviour : m_activeBehaviours)
			{
				behaviour->CallMessage(messageName, std::forward<Args>(args)...);
			}
			for (auto& behaviour : m_inactiveBehaviours)
			{
				behaviour->CallMessage(messageName, std::forward<Args>(args)...);
			}
		}

		void CheckAndSortBehaviours();
		bool StartUp(const std::string& userScriptsDLLPath);
		void ShutDown();
	};
}