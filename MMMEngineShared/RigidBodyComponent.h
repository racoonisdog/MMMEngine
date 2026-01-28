#pragma once
#include <physx/PxPhysicsAPI.h>
#include "Component.h"
#include "SimpleMath.h"
#include "Export.h"
#include "rttr/type"
#include "Delegates.hpp"
#include <iostream>

using namespace DirectX::SimpleMath;

namespace MMMEngine {
	class ColliderComponent;
}

namespace MMMEngine {
	class MMMENGINE_API RigidBodyComponent : public Component
	{
	private:
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
	public:
		//물체의 타입
		enum class Type
		{
			Static,
			Dynamic
		};

		//가하는 힘의 종류
		enum class ForceMode
		{
			Force,
			Impulse,
			VelocityChange,
			Acceleration
		};

		struct Desc
		{
			Type type = Type::Dynamic;
			float mass = 1.0f;
			//선형감쇠수치 , 공기저항, 속도에 비례해서 감소
			float linearDamping = 0.0f;
			//각속도 감쇠 수치, 회전 관성을 줄임(회전이 점점 느려지고 멈춘다)
			float angularDamping = 0.05f;
			bool useGravity = true;
			bool isKinematic = false;
			
			Desc() = default;

			Desc(Type _type, float _mass, float _linearDamping, float _angularDamping, bool _useGravity, bool _isKinematic) :
			type(_type), mass(_mass) , linearDamping(_linearDamping), angularDamping(_angularDamping), useGravity(_useGravity), isKinematic(_isKinematic)
			{}
		};



	public:
		RigidBodyComponent() = default;
		RigidBodyComponent(const Desc& desc) : m_Desc(desc) {};

		void Initialize() override;	//생성자 이후 추가 초기화용
		void UnInitialize()override;

		void OnDestroy();
		//Scene에 등록
		void CreateActor(physx::PxPhysics* physics, DirectX::SimpleMath::Vector3 worldPos, DirectX::SimpleMath::Quaternion Quater);

		//Shape 붙이기
		void AttachCollider(ColliderComponent* collider);

		void DetachCollider(ColliderComponent* collider);

		physx::PxRigidActor* GetActor() const { return m_Actor; }

		void DestroyActor();

		//physics에 물리정보를 던져주는 용도
		void PushToPhysics();
		//simulate이후 getGlobalPose()일어서 m_Tr에 반영하기 위한 함수
		void PullFromPhysics();

		//좌표를 강제로 바꿨을때 dirty설정 ( 외부적 요인으로 인해 변경했을때만 호출 )
		void PushPoseIfDirty();

		void PushForces();

		//Dynamic이지만 회전은 즉각 변경하는 로직
		void SnapRotation(const Quaternion& q);


		void Teleport(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Quaternion& Quater);
		void SetKinematicTarget(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Quaternion& Quater);
		void MoveKinematicTarget();
		void Editor_changeTrans(const DirectX::SimpleMath::Vector3& worldPos, const DirectX::SimpleMath::Quaternion& Quater);

		//힘, 속도 등 조작용 API
		void AddForce(DirectX::SimpleMath::Vector3 f, ForceMode mod);
		void AddTorque(DirectX::SimpleMath::Vector3 tor, ForceMode mod);

		//rigid값 설정시 physx에 반영하는 함수
		void PushStateChanges();

		//sleep/wake 시스템
		void PushWakeUp();

		void AddImpulse(DirectX::SimpleMath::Vector3 imp);

		void SetLinearVelocity(DirectX::SimpleMath::Vector3 v);

		DirectX::SimpleMath::Vector3 GetLinearVelocity() const;

		void SetAngularVelocity(DirectX::SimpleMath::Vector3 w);

		DirectX::SimpleMath::Vector3 GetAngularVelocity() const;

		void SetisAutoRigid(bool value);
		bool GetisAutoRigid();

		void WakeUp();


		bool GetUseGravity() { return m_Desc.useGravity; }
		bool GetKinematic() { return m_Desc.isKinematic; }
		float GetMass() { return m_Desc.mass; }
		float GetLineDamping() { return m_Desc.linearDamping; }
		float GetAngularDamping() { return m_Desc.angularDamping; }
		Type GetType() { return m_Desc.type; }


		void SetUseGravity(bool value);
		void SetKinematic(bool value);
		void SetMass(float mass) { m_Desc.mass = mass; m_DescDirty = true; m_WakeRequested = true; }
		void SetLineDamping(float lin) { m_Desc.linearDamping = lin; m_DescDirty = true; m_WakeRequested = true; }
		void SetAngularDamping(float ang) { m_Desc.angularDamping = ang, m_DescDirty = true; m_WakeRequested = true; }
		void SetType(Type newType);

		physx::PxRigidActor* GetPxActor() const;


		Vector3 GetRequestedPos() { return m_RequestedPos; }
		Quaternion GetRequestedRot() { return m_RequestedRot; }
		void AttachShapeOnly(physx::PxShape* shape);
		void SetType_Internal();
		bool HasPendingTypeChange();
		void OffPendingType();


		//모델의 물리영역에서의 worldpose, forward, yaw 반환 함수
		Vector3 Px_GetWorldPosition() const;

		Quaternion Px_GetWorldRotation() const;

		// 바라보는 방향 (월드 기준, y=0 보정 포함 권장)
		Vector3 Px_GetForward() const;

		// 요 각도 (라디안)
		float Px_GetYaw() const;

	private:
		Desc m_Desc;

		//내부적으로만 쓸 위치객체
		struct Pose
		{
			DirectX::SimpleMath::Vector3 position;
			DirectX::SimpleMath::Quaternion rotation;
		};

		//Actor변수
		physx::PxRigidActor* m_Actor = nullptr;
		//collider 리스트
		//std::vector<ColliderComponent*> m_PendingColliders;
		std::vector<ColliderComponent*> m_Colliders;

		//Transform이 바뀌었다는 flag용도 ( 텔레포트, 오브젝트를 직접 끌어서 이동, 키네마틱 이동( 플래폼, 컷씬 경로 이동 등 ), 스크립트에 의한 이동)
		bool m_PoseDirty = false;
		//텔레포트, 엔진이 강제로 바꾼 trans용도
		Pose m_RequestedWorldPose;

		//키네마틱 이동용
		bool m_HasKinematicTarget = false;
		//setKinematictarget으로 보낼값
		Pose m_KinematicTarget;


		// 상태 플래그 ( desc를 런타임이나 에디터에서 변경시 true로됨)
		bool m_DescDirty = false;
		// 물체를 끌어다 놓거나 텔레포트했을때 wake 알려주도록 설정하는 flag값
		bool m_WakeRequested = false;


		//force, torque 설정 변수
		//바로 처리하는게 아니라 컨테이너에 담아두고 함수를 통해 순차적으로 실행하도록
		struct ForceCmd
		{
			DirectX::SimpleMath::Vector3 vec;
			ForceMode mode;
		};

		struct TorqueCmd
		{
			DirectX::SimpleMath::Vector3 vec;
			ForceMode mode;
		};

		std::vector<ForceCmd>  m_ForceQueue;
		std::vector<TorqueCmd> m_TorqueQueue;

		physx::PxPhysics* m_Physics = nullptr;

		bool m_IsAutoRigid = false;

		void BindTeleport();

		//settype용함수
		bool m_TypeChangePending = false;
		Type m_RequestedType = Type::Dynamic;
		Vector3 m_RequestedPos{};
		Quaternion m_RequestedRot{};
	private:
		physx::PxForceMode::Enum ToPxForceMode(ForceMode mode);

		//collider 전체 삭제에 대한 책임을 가지는 단 하나의 rigid 보장 bool flag
		bool m_ColliderMaster = false;
	};
}
