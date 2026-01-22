#include "PhysX.h"
#include <physx/extensions/PxDefaultCpuDispatcher.h>
#include <physx/cooking/PxCooking.h>
#include <iostream>
//#include <physx/pvd/PxPvd.h>
//#include <physx/pvd/PxPvdTransport.h>
#include "PhysXHelper.h"
#include <physx/extensions/PxExtensionsAPI.h>

DEFINE_SINGLETON(MMMEngine::PhysicX)

//순서 보장을 위해 init 함수를 개별로 나눔
bool MMMEngine::PhysicX::Initialize()
{
	if (!InitTolerances(1.0f, 1.0f)) return false;
	if (!InitFoundation()) return false;
//#if defined(_DEBUG)
//	if (!InitPVD()) return false;
//#endif
	if (!InitPhysics()) return false;
	if (!InitCook()) return false;
	//if (!InitDispatcher()) return false;
	if (!InitMaterial()) return false;

	return true;
}

bool MMMEngine::PhysicX::UnInitialize()
{
	SAFE_RELEASE(m_physics);
	//SAFE_RELEASE(m_dispatcher);
//#if defined(_DEBUG)
//	if (m_Pvd) m_Pvd->disconnect();
//	SAFE_RELEASE(transport);
//	SAFE_RELEASE(m_Pvd);
//#endif
	//마지막에 해제
	SAFE_RELEASE(m_foundation);
	SAFE_RELEASE(m_defaultMaterial);
	return true;
}

physx::PxMaterial* MMMEngine::PhysicX::GetDefaultMaterial()
{
	return m_defaultMaterial;
}

physx::PxMaterial* MMMEngine::PhysicX::GetMaterial(MaterialID id)
{
	return m_defaultMaterial;
}

bool MMMEngine::PhysicX::InitTolerances(float _length, float _speed)
{
	if (_length <= 0.01f || _length > 100.0f) { std::cout << "잘못된 길이값 입력" << std::endl; return false; }
	if (_speed <= 0.01f) { std::cout << "잘못된 속도값 입력" << std::endl; return false; }

	m_Tscale.length = _length; m_Tscale.speed = _speed;
	return true;
}

bool MMMEngine::PhysicX::InitFoundation()
{
	m_foundation = PxCreateFoundation(PX_PHYSICS_VERSION, m_allocator, m_errorCallback);
	if (m_foundation == nullptr) return false;
	return true;
}

bool MMMEngine::PhysicX::InitPhysics()
{
	physx::PxTolerancesScale scale;
	scale.length = m_Tscale.length; scale.speed = m_Tscale.speed;
	m_physics = PxCreatePhysics(PX_PHYSICS_VERSION, *m_foundation, scale, true, nullptr);
	if (m_physics == nullptr) return false;
	//if(!PxInitExtensions(*m_physics, m_Pvd ))
	return true;
}

//bool MMMEngine::PhysicX::InitDispatcher()
//{
//	m_dispatcher = physx::PxDefaultCpuDispatcherCreate(threadCount);
//	if (m_dispatcher == nullptr) return false;
//	return true;
//}

bool MMMEngine::PhysicX::InitCook()
{
	//3, 4 버전에서는 상태풀 객체였지만 5에서는 순수 함수화를 했기때문에 설정값 + 입력 => 결과로 단순화함
	//m_cookingParams = physx::PxCookingParams(m_physics->getTolerancesScale());
	m_cookingParams.emplace(m_physics->getTolerancesScale());
	m_cookingParams->meshPreprocessParams |= physx::PxMeshPreprocessingFlag::eWELD_VERTICES;
	m_cookingParamInitialized = true;
	return true;
}

bool MMMEngine::PhysicX::InitMaterial()
{
	m_defaultMaterial = m_physics->createMaterial(
		0.5f,   // static friction
		0.5f,   // dynamic friction
		0.1f    // restitution
	);
	if (m_defaultMaterial == nullptr) return false;
	return true;
}

//디버그시에만 사용
//bool MMMEngine::PhysicX::InitPVD()
//{
//	//PVD 연결
//	m_Pvd = physx::PxCreatePvd(*m_foundation);
//	if (!m_Pvd) return false;
//	transport = physx::PxDefaultPvdSocketTransportCreate("127.0.0.1", 5425, 10);
//	if (transport == nullptr) return false;
//	if (!m_Pvd->connect(*transport, physx::PxPvdInstrumentationFlag::eALL)) return false;
//	return true;
//}
