#pragma once
#include "Scene.h"
#include "TimeManager.h"
#include "PhysX.h"
#include "RigidBodyComponent.h"
#include "ColliderComponent.h"
#include "PhysScene.h"
#include "ExportSingleton.hpp"
#include "CollisionMatrix.h"


#include <iostream>
#include <variant>



namespace MMMEngine
{
	enum class CollisionPhase { Enter, Stay, Exit };
	enum class TriggerPhase { Enter, Exit };

	struct MMMENGINE_API CollisionInfo
	{
		ObjPtr<GameObject> self;
		ObjPtr<GameObject> other;

		Vector3 normal;          // self 기준 (중요)
		Vector3 point;           // 월드 접촉점(대표값)
		float penetrationDepth;  // >= 0

		ObjPtr<ColliderComponent> selfCollider = nullptr;
		ObjPtr<ColliderComponent> otherCollider = nullptr;

		bool isTrigger = false; // collision이면 false

		CollisionPhase phase = CollisionPhase::Stay;
	};

	struct MMMENGINE_API TriggerInfo
	{
		ObjPtr<GameObject> self;
		ObjPtr<GameObject> other;

		ObjPtr<ColliderComponent> selfCollider = nullptr;
		ObjPtr<ColliderComponent> otherCollider = nullptr;

		bool isEnter = false;

		TriggerPhase phase = TriggerPhase::Enter;
	};

	class MMMENGINE_API PhysxManager : public Utility::ExportSingleton<PhysxManager>
	{
	public:
		// 현재 씬 바인딩 (Scene이 PhysScene을 보유함)
		void BindScene(MMMEngine::Scene* scene);

		//rigid , collider 등록
		void SetStep();

		// fixed step에서 호출되는 진입점
		void StepFixed(float dt);
		void ApplyInterpolation(float alpha);
		void SyncRigidsFromTransforms();

		//외부 노출함수
		void NotifyRigidAdded(RigidBodyComponent* rb);
		void NotifyRigidRemoved(RigidBodyComponent* rb);

		void NotifyColliderAdded(ColliderComponent* col);
		void NotifyColliderRemoved(ColliderComponent* col);

		// 값 변경 
		void NotifyColliderChanged(ColliderComponent* col);

		// 타입 변경
		void NotifyRigidTypeChanged(RigidBodyComponent* rb);

		void UnbindScene();
		
		void DispatchPhysicsEvents();

		MMMEngine::PhysScene* getPScene() { return &m_PhysScene; }

		

		void SetSceneGravity(float x, float y, float z);


		void SetLayerCollision(uint32_t layerA, uint32_t layerB, bool canCollide);

		std::vector<std::variant<CollisionInfo, TriggerInfo>>& GetCallbackQue() { return Callback_Que; }

		std::vector<ColliderComponent*> m_PendingDestroyCols;

	private:
		// 내부에서만 쓰는 헬퍼
		void FlushCommands_PreStep();
		void FlushCommands_PostStep();
		void ApplyFilterConfigIfDirty();
		void FlushDirtyColliders_PreStep();
		void FlushDirtyColliderFilters_PreStep();

		//헬퍼함수
		void EraseCommandsForRigid(MMMEngine::RigidBodyComponent* rb);
		void EraseAttachDetachForRigid(MMMEngine::RigidBodyComponent* rb);
		void EraseCommandsForCollider(MMMEngine::ColliderComponent* col);

		// 등록/해제 요청 ( 노출 x 내부에서 사용 )
		void RequestRegisterRigid(MMMEngine::RigidBodyComponent* rb);
		void RequestUnregisterRigid(MMMEngine::RigidBodyComponent* rb);

		void RequestAttachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col);
		void RequestDetachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col);

		void RequestRebuildCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col);
		void RequestReapplyFilters(); // 씬 설정 바뀌었을 때(Dirty)

		void RequestChangeRigidType(MMMEngine::RigidBodyComponent* rb);

		// 자동 rigid 생성 헬퍼함수
		MMMEngine::RigidBodyComponent* GetOrCreateRigid(ObjPtr<GameObject> go);
		bool HasAnyCollider(ObjPtr<GameObject> go) const;

		//이벤트보관함수
		//std::vector<std::tuple<ObjPtr<GameObject>, ObjPtr<GameObject>, P_EvenType>> Callback_Que;
		std::vector<std::variant<CollisionInfo, TriggerInfo>> Callback_Que;

		void Shutdown();
	private:
		// 주입 ,바인딩
		MMMEngine::Scene* m_Scene = nullptr;              // 현재 씬(소유 X)

		//필터 정책
		MMMEngine::CollisionMatrix m_CollisionMatrix;     // 레이어/마스크 정책
		bool m_FilterDirty = false;            // 에디터에서 설정 변경 시 true

		//명령 큐
		enum class CmdType : uint8_t
		{
			RegRigid,       //RigidBody를 physScene에 등록하는 명령 //scene->addActor(actor)
			UnregRigid,     //RigidBody를 physScene에 제거하는 명령 //scene->removeActor(actor)
			AttachCol,      //이 RigidBody에 이 Collider에 붙여라  //actor->attachShape(shape)
			DetachCol,      //이 Collider를 actor에서 떼어놔라       //actor->detachShape(shape)
			RebuildCol,     //이 Collider의 shape를 다시 만들어라 //새 geometry로 BuildShape -> 다시 attach
			ChangeRigid		//RigidBody의 타입은 변경하라
		};

		struct Command
		{
			CmdType type;
			MMMEngine::RigidBodyComponent* new_rb = nullptr;
			MMMEngine::ColliderComponent* col = nullptr;
		};

		std::vector<Command> m_Commands;

		MMMEngine::PhysScene m_PhysScene;

		//형태가 변한 콜리더를 담아두는 벡터
		std::unordered_set<MMMEngine::ColliderComponent*> m_DirtyColliders;

		//Unregister가 예약된 rigid만 담는 컨테이너( 제거된게 아닌 제거될 예정인 rigid )
		std::unordered_set<RigidBodyComponent*> m_PendingUnreg;

		bool m_IsInitialized = false;


		std::unordered_set<ColliderComponent*> m_FilterDirtyColliders;
	};
}
