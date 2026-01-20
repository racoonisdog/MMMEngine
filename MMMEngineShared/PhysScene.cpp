#include "PhysScene.h"
#include "PhysXHelper.h"
#include "PhysicsFilter.h"

bool MMMEngine::PhysScene::Create(const PhysSceneDesc& desc)
{
	m_desc = desc;

	//Dispatcher(thread 설정)
	uint32_t threads = (m_desc.cpuThreadCount == 0) ? 2u : m_desc.cpuThreadCount;
	m_dispatcher = physx::PxDefaultCpuDispatcherCreate(threads);

	if (!m_dispatcher) return false;

	if (!CreateScene())
	{
		SAFE_RELEASE(m_dispatcher);
		return false;
	}

	//if (m_desc.enablePVD) {
	//	//SetupPvdFlags();
	//}

	return true;
}

bool MMMEngine::PhysScene::CreateScene()
{
	auto& physics = PhysicX::Get().GetPhysics();

	physx::PxSceneDesc pxDesc(physics.getTolerancesScale());
	pxDesc.gravity = ToPxVec(m_desc.gravity);
	pxDesc.cpuDispatcher = m_dispatcher;
	pxDesc.filterShader = CustomFilterShader;
	pxDesc.simulationEventCallback = &m_callback;

	m_scene = PhysicX::Get().GetPhysics().createScene(pxDesc);
	if (m_scene == nullptr) return false;

	return true;
}

void MMMEngine::PhysScene::Destroy()
{
	if (m_scene)
	{
		m_scene->release();
		m_scene = nullptr;
	}

	if (m_dispatcher)
	{
		m_dispatcher->release();
		m_dispatcher = nullptr;
	}
}

void MMMEngine::PhysScene::Step(float dt)
{
	if (!m_scene) return;
	if (dt <= 0.0f) return;
	m_scene->simulate(dt);
	m_scene->fetchResults(true);

	//콜백이벤트 drain ( 비우기 )
	m_frameContacts.clear(); m_frameTriggers.clear();

	m_callback.DrainContacts(m_frameContacts);
	m_callback.DrainTriggers(m_frameTriggers);
}

void MMMEngine::PhysScene::AddActor(physx::PxActor& actor)
{
	if (!m_scene) return;
	m_scene->addActor(actor);
}

void MMMEngine::PhysScene::RemoveActor(physx::PxActor& actor)
{
	if (!m_scene) return;
	m_scene->removeActor(actor);
}

void MMMEngine::PhysScene::RegisterRigid(MMMEngine::RigidBodyComponent* rb, MMMEngine::PhysicX& core)
{

}

void MMMEngine::PhysScene::UnregisterRigid(MMMEngine::RigidBodyComponent* rb)
{

}

void MMMEngine::PhysScene::AttachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col, MMMEngine::PhysicX& core, const CollisionMatrix& matrix)
{

}

void MMMEngine::PhysScene::DetachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col)
{

}

void MMMEngine::PhysScene::RebuildCollider(MMMEngine::ColliderComponent* col, MMMEngine::PhysicX& core, const CollisionMatrix& matrix)
{

}

void MMMEngine::PhysScene::ReapplyFilters(const CollisionMatrix& matrix)
{

}
