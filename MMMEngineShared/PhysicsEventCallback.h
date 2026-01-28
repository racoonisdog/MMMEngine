#pragma once
#include <physx/PxPhysicsAPI.h>
#include <iostream>
#include <vector>
#include <mutex>
#include "Export.h"


namespace MMMEngine
{
	class MMMENGINE_API PhysXSimulationCallback : public physx::PxSimulationEventCallback
	{
	public:
		~PhysXSimulationCallback() override = default;

		//Trigger가 아닌 ‘물리 접촉(Contact)’에서 발생, 두 액터가 실제로 물리적으로 맞닿았을 때 충돌 시작 / 유지 / 종료 상태를 담기 위한 이벤트 데이터
		struct ContactEvent
		{
			physx::PxActor* a;
			physx::PxActor* b;

			physx::PxShape* aShape;
			physx::PxShape* bShape;

			physx::PxU32 events;

			physx::PxVec3 normal{ 0, 0, 0 };     // A 기준 대표 noraml
			physx::PxVec3 point{ 0, 0, 0 };      // 대표 contact point
			float penetrationDepth = 0.0f;   // >= 0 (침투 깊이)

			ContactEvent(physx::PxActor* _a, physx::PxActor* _b, physx::PxShape* _ashape , physx::PxShape* _bshape, physx::PxU32 _events,
				const physx::PxVec3& _normal, const physx::PxVec3& _point, float _depth)
				: a(_a), b(_b), aShape(_ashape), bShape(_bshape), events(_events), normal(_normal), point(_point), penetrationDepth(_depth) {
			}
		};
		//events에는 Found, Persists, lost 같은 상태 존재

		
		//트리거 객체, 일반객체를 담는 컨테이너
		struct TriggerEvent
		{
			physx::PxActor* triggerActor;
			physx::PxActor* otherActor;

			physx::PxShape* triggerShape;
			physx::PxShape* otherShape;

			bool isEnter;			
		};
		//physx::PxU32 events; 트리거에서 events를 안쓰는 이유는 지금단계에서는 event enter/exit로만 나눴음

		//콜백 내부에서는 수집만 함

		void onContact(const physx::PxContactPairHeader& pairHeader, const physx::PxContactPair* pairs, physx::PxU32 nbPairs) override;

		void onTrigger(physx::PxTriggerPair* pairs, physx::PxU32 count) override;

		
		//담아둔 함수들을 꺼내서 쓰기위한 Get함수
		const std::vector<ContactEvent>& GetContacts() const { return m_contacts; }
		const std::vector<TriggerEvent>& GetTriggers() const { return m_triggers; }

		void DrainContacts(std::vector<ContactEvent>& out);

		void DrainTriggers(std::vector<TriggerEvent>& out);

		void Clear();



		//아래는 이번 프로젝트에서 사용 안하는 용도
		//constrainbreak의 경우 로프끊김, 부러지는 연결, 래그돌 관절 파손등에 사용 ( 이번 프로젝트 사용 안함 )
		//constraint는 두 물체를 특정 규칙으로 묶는장치 - 이때 일정이상 힘을 주면 끊어지도록 임계치 설정하는데 임계치를 넘으면 이 이벤트 호출
		void onConstraintBreak(physx::PxConstraintInfo*, physx::PxU32) override {};

		//성능최적화, 디버그 용도로 사용
		void onWake(physx::PxActor**, physx::PxU32) override {};
		void onSleep(physx::PxActor**, physx::PxU32) override {};

		//시뮬레이션 결과 pose가 준비된 시점에 결과 pose 배열을 콜백으로 넘겨주는 메커니즘
		void onAdvance(const physx::PxRigidBody* const*, const physx::PxTransform*, const physx::PxU32) override {};
	private:
		std::mutex m_mtx;
		std::vector<ContactEvent> m_contacts;
		std::vector<TriggerEvent> m_triggers;
	};
}
