#include "BehaviourManager.h"

DEFINE_SINGLETON(MMMEngine::BehaviourManager)

void MMMEngine::BehaviourManager::CheckAndSortBehaviours()
{
	if (m_needSort)
	{
		SortBehaviours();
		m_needSort = false;
	}
}

bool MMMEngine::BehaviourManager::StartUp(const std::string& userScriptsDLLPath)
{
	m_pScriptLoader = std::make_unique<ScriptLoader>();
	return m_pScriptLoader->LoadScriptDLL(userScriptsDLLPath);
}

void MMMEngine::BehaviourManager::ShutDown()
{
	m_pScriptLoader.release();
	m_activeBehaviours.clear();
	m_inactiveBehaviours.clear();
}

void MMMEngine::BehaviourManager::RegisterBehaviour(ObjPtr<Behaviour> behaviour)
{
	m_inactiveBehaviours.push_back(behaviour);
	m_firstCallBehaviours.insert(behaviour);
	m_needSort = true; // Behaviour 정렬이 필요함을 표시
}

void MMMEngine::BehaviourManager::UnRegisterBehaviour(ObjPtr<Behaviour> behaviour)
{
	auto it = std::find(m_activeBehaviours.begin(), m_activeBehaviours.end(), behaviour);
	if (it != m_activeBehaviours.end())
	{
		m_activeBehaviours.erase(it);
	}
	else
	{
		it = std::find(m_inactiveBehaviours.begin(), m_inactiveBehaviours.end(), behaviour);
		if (it != m_inactiveBehaviours.end())
		{
			m_inactiveBehaviours.erase(it);
		}
	}
	m_needSort = true; // Behaviour 정렬이 필요함을 표시
}

void MMMEngine::BehaviourManager::SortBehaviours()
{
	std::sort(m_activeBehaviours.begin(), m_activeBehaviours.end(),
		[](ObjPtr<Behaviour> a, ObjPtr<Behaviour> b) {
			return a->m_executionOrder < b->m_executionOrder;
		});
}

void MMMEngine::BehaviourManager::AllSortBehaviours()
{
	std::sort(m_activeBehaviours.begin(), m_activeBehaviours.end(),
		[](ObjPtr<Behaviour> a, ObjPtr<Behaviour> b) {
			return a->m_executionOrder < b->m_executionOrder;
		});

	std::sort(m_inactiveBehaviours.begin(), m_inactiveBehaviours.end(),
		[](ObjPtr<Behaviour> a, ObjPtr<Behaviour> b) {
			return a->m_executionOrder < b->m_executionOrder;
		});
}

void MMMEngine::BehaviourManager::InitializeBehaviours()
{
	// 활성화된 Behaviour를 모으는 컨테이너.
// count() 메서드로 O(1)에 존재 여부 확인 가능.
	std::unordered_set<ObjPtr<Behaviour>> changedBehavioursSet;

	// 이전에 생성되어 활성화된 적 없는 Behaviour를 효율적으로 찾기 위한 set
	std::unordered_set<ObjPtr<Behaviour>> newBehavioursSet;

	// m_inactiveBehaviours를 순회하며 활성화 객체 처리
	auto it = m_inactiveBehaviours.begin();
	while (it != m_inactiveBehaviours.end())
	{
		ObjPtr<Behaviour> currentBehaviour = *it;
		if (currentBehaviour->IsActiveAndEnabled())
		{
			m_activeBehaviours.push_back(currentBehaviour);
			changedBehavioursSet.insert(currentBehaviour); // OnEnable 호출을 위해 추가

			// m_firstCallBehaviours에서 찾고, newBehavioursSet에 추가 및 m_firstCallBehaviours에서 제거
			if (m_firstCallBehaviours.erase(currentBehaviour) > 0) // 요소가 성공적으로 제거되면
			{
				newBehavioursSet.insert(currentBehaviour); // 새로운 Behaviour로 간주
			}
			m_needSort = true;

			// m_inactiveBehaviours에서 현재 요소를 제거
			it = m_inactiveBehaviours.erase(it);
		}
		else
		{
			++it;
		}
	}

	// 활성화 behaviour 정렬 (필요시에)
	CheckAndSortBehaviours();

	// 1. Awake 호출 (새로운 객체만)
	for (auto& behaviour : m_activeBehaviours)
	{
		if (newBehavioursSet.count(behaviour) > 0)
		{
			behaviour->CallMessage("Awake");
		}
	}

	// 2. OnEnable 호출 (새롭게 활성화된 모든 객체)
	for (auto& behaviour : m_activeBehaviours)
	{
		if (changedBehavioursSet.count(behaviour) > 0)
		{
			behaviour->CallMessage("OnEnable");
		}
	}

	// 3. Start 호출 (새로운 객체만)
	for (auto& behaviour : m_activeBehaviours)
	{
		if (newBehavioursSet.count(behaviour) > 0)
		{
			behaviour->CallMessage("Start");
		}
	}
}

void MMMEngine::BehaviourManager::DisableBehaviours()
{
	auto it = m_activeBehaviours.begin();
	while (it != m_activeBehaviours.end())
	{
		ObjPtr<Behaviour> currentBehaviour = *it;
		if (!currentBehaviour->IsActiveAndEnabled() || currentBehaviour->IsDestroyed())
		{
			(*it)->CallMessage("OnDisable");
			m_needSort = true;

			m_inactiveBehaviours.push_back(currentBehaviour); // 바로 m_inactiveBehaviours로 이동
			it = m_activeBehaviours.erase(it); // m_activeBehaviours에서 제거
		}
		else
		{
			++it;
		}
	}

	for (auto& inactive : m_inactiveBehaviours)
	{
		if (inactive->IsDestroyed())
		{
			inactive->CallMessage("OnDestroy");
		}
	}
}

void MMMEngine::BehaviourManager::BroadCastBehaviourMessage(const std::string& messageName)
{
	for (auto& behaviour : m_activeBehaviours)
	{
		behaviour->CallMessage(messageName);
	}
}

void MMMEngine::BehaviourManager::ReloadUserScripts(const std::string& name)
{
	m_pScriptLoader->LoadScriptDLL(name);
}


