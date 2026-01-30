#pragma once
#include "PhysX.h"
#include "PhysicsEventCallback.h"
#include "RigidBodyComponent.h"
#include "ColliderComponent.h"
#include "CollisionMatrix.h"

//using namespace DirectX::SimpleMath;


//Todo : 터지면 MMMEngine 걸어두기
struct PhysSceneDesc
{
	//Vec3 gravity = { 0.f, -9.81f, 0.f };
	float gravity[3] = { 0.f, 0.f, 0.f };

	uint32_t cpuThreadCount = 0;
	//총알같은 가속옵션 필요할때 true 및 설정
	bool enableCCD = false;
	//pvd 현 프로젝트에서는 안씀
	bool enablePVD = false;

	//충돌 필터 설정
	physx::PxSimulationFilterShader userFilterShader = nullptr;
	//사용자 커스텀 콜백 이벤트 , 현 프로젝트는 쓸예정 없음
	physx::PxSimulationEventCallback* userEventCallback = nullptr;
};

namespace MMMEngine
{
	class MMMENGINE_API PhysScene
	{
	public:
		PhysScene() = default;
		~PhysScene() { Destroy(); };

		bool Create(const PhysSceneDesc& desc);
		bool CreateScene();

		void Destroy();

		//시뮬레이션을 한 프레임 진행시키는 함수 //simulate 계산 시작, fetchResults 계산이 끝날때까지 대기 //멀티스레드
		void Step(float dt);

		void PullRigidsFromPhysics();
		void ApplyInterpolation(float alpha);
		void SyncRigidsFromTransforms();

		void DrainEvents();

		//PxRigidActor를 PxScene에 등록 // add한순간부터 물리가 적용
		void AddActor(physx::PxActor& actor);
		//Scene에서 actor를 빼는 함수  //Scene에서 빠지면 더이상 시뮬안함
		void RemoveActor(physx::PxActor& actor);

		physx::PxScene& GetScene() { return *m_scene; }

		//PhysScene는 하나만 존재해야함 ( 싱글톤 구현이 아니라 복사 막아두기 필요 )
		PhysScene(const PhysScene&) = delete;
		PhysScene& operator=(const PhysScene&) = delete;
		PhysScene(PhysScene&&) = delete;
		PhysScene& operator=(PhysScene&&) = delete;

		
		//등록/해제/부착/분리/리빌드
		void RegisterRigid(MMMEngine::RigidBodyComponent* rb);
		void UnregisterRigid(MMMEngine::RigidBodyComponent* rb);

		//void SwapRigid(RigidBodyComponent* oldRb, RigidBodyComponent* newRb, const CollisionMatrix& matrix);

		void AttachCollider(MMMEngine::RigidBodyComponent* rb,
			MMMEngine::ColliderComponent* col,
			const CollisionMatrix& matrix);

		void DetachCollider(MMMEngine::RigidBodyComponent* rb,
			MMMEngine::ColliderComponent* col);

		void ReapplyFilters(const CollisionMatrix& matrix);

		//콜리더 크기 변경
		void UpdateColliderGeometry(MMMEngine::ColliderComponent* col);

		//콜리더 재부착
		void RebuildCollider(MMMEngine::ColliderComponent* col, const CollisionMatrix& matrix);

		//현재 씬에 등록된 rigid들에 대해 pushtophysics 호출용
		void PushRigidsToPhysics();

		void ChangeRigidType(MMMEngine::RigidBodyComponent* col, const CollisionMatrix& matrix);

		const std::vector<MMMEngine::PhysXSimulationCallback::ContactEvent>& GetFrameContacts() const { return m_frameContacts; }
		const std::vector<PhysXSimulationCallback::TriggerEvent>& GetFrameTriggers() const { return m_frameTriggers; }

		//맵 중력 셋팅
		void SetGravity(float x, float y, float z);

		void ResetFilteringFor(MMMEngine::ColliderComponent* col);

	private:
		PhysSceneDesc m_desc;

		physx::PxScene* m_scene = nullptr;
		physx::PxDefaultCpuDispatcher* m_dispatcher = nullptr;

		MMMEngine::PhysXSimulationCallback m_callback;

		std::vector<MMMEngine::PhysXSimulationCallback::ContactEvent> m_frameContacts;
		std::vector<MMMEngine::PhysXSimulationCallback::TriggerEvent> m_frameTriggers;


		//해당 scene에서 사용되는 rigid 목록
		std::unordered_set<MMMEngine::RigidBodyComponent*> m_rigids;
		
		//해당 shape가 어느 actor에 붙어있는지 ( 어떤 collider가 rigid에 붙었는지 )
		std::unordered_map< MMMEngine::ColliderComponent*, MMMEngine::RigidBodyComponent*> m_ownerByCollider;

		// Rigid가 죽거나 제거할때 일괄적으로 detach처리하기 위한 컨테이너 ( 임시 보관 컨테이너 )
		std::unordered_map<MMMEngine::RigidBodyComponent*, std::vector<MMMEngine::ColliderComponent*>> m_collidersByRigid;
	};
}