#pragma once
#include <optional>
#include "ExportSingleton.hpp"
#include <physx/PxPhysicsAPI.h>
#include <unordered_map>


struct TolerancesScale
{
	float length;
	float speed;
};

namespace MMMEngine
{
	class MMMENGINE_API PhysicX : public Utility::ExportSingleton<PhysicX>
	{
	public:
		using MaterialID = uint32_t;

		//PhysicX() : m_cookingParams(physx::PxTolerancesScale()) {};
		PhysicX() = default;
		~PhysicX() = default;

		bool Initialize();
		bool UnInitialize();

		TolerancesScale m_Tscale{};

		physx::PxFoundation& GetFoundation() { return *m_foundation; }
		const physx::PxFoundation& GetFoundation() const { return *m_foundation; }

		physx::PxPhysics& GetPhysics() { return *m_physics; }
		const physx::PxPhysics& GetPhysics() const { return *m_physics; }

		//const physx::PxCpuDispatcher& GetDispatcher() const { return *m_dispatcher; }

		physx::PxCookingParams& GetCookParam() { return m_cookingParams.value(); }
		const physx::PxCookingParams& GetCookParam() const { return m_cookingParams.value(); }

		//physx::PxPvd& GetPvd() { return *m_Pvd; }
		//const physx::PxPvd& GetPvd() const { return *m_Pvd; }

		physx::PxMaterial* GetDefaultMaterial();
		physx::PxMaterial* GetMaterial(MaterialID id);

	private:
		//PhysX SDK의 기반/코어 서비스 관리 객체, 메모리 할당/해체 정책 관리, 에러/로그 콜백 관리, 공용 리소스/내부 인프라 초기화
		//가장 먼저 생성되어야하고 가장 마지막에 파괴되어야
		physx::PxFoundation* m_foundation = nullptr;
		physx::PxDefaultAllocator m_allocator{};
		physx::PxDefaultErrorCallback m_errorCallback{};

		//실제 물리 엔진 코어 ( PxScene, PxMaterial, PxRigidActor 등을 생성하는 팩토리 )
		physx::PxPhysics* m_physics = nullptr;

		//PhysX 내부 멀티스레드 작업 분배용 (world로 내리는것도 검토***)
		//physx::PxDefaultCpuDispatcher* m_dispatcher = nullptr;

		//런타임 메시 충돌을 위한 콜라이더 제작 최적화용 cook
		//physx::PxCookingParams m_cookingParams;
		std::optional<physx::PxCookingParams> m_cookingParams;
		bool m_cookingParamInitialized = false;

		//PVD, 디버그
		//physx::PxPvd* m_Pvd = nullptr;
		//physx::PxPvdTransport* transport = nullptr;


		physx::PxMaterial* m_defaultMaterial;
		std::unordered_map<MaterialID, physx::PxMaterial*> m_materials;

		int threadCount;

		bool InitTolerances(float _length, float _speed);
		bool InitFoundation();
		bool InitPhysics();
		//bool InitDispatcher();
		bool InitCook();
		bool InitMaterial();
		//bool InitPVD();
	};
}
