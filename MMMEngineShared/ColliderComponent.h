#pragma once
#include <physx/PxPhysicsAPI.h>


class ColliderComponent
{
public:
	virtual ~ColliderComponent()
	{
		if (m_Shape && m_Owned) { m_Shape->release(); }
		m_Shape = nullptr;
	};

	enum class ShapeMode : uint8_t
	{
		Simulation,   // 물리 충돌(접촉/반작용)
		Trigger,      // 오버랩 이벤트만(밀림/반작용 없음)
		QueryOnly,    // 레이캐스트/스윕/오버랩에만 잡힘(물리/트리거 이벤트 없음)
		Disabled      // 아무것도 안 함(임시로 끄기용)
	};

	// 콜라이더 종류별로 shape 만드는 가상함수
	virtual void BuildShape(physx::PxPhysics* physics, physx::PxMaterial* material) = 0;


	physx::PxShape* GetPxShape() const { return m_Shape; }
	ShapeMode GetShapeMode() const { return m_Mode; }


	// 공통 설정(Shape 생성 전/후 둘 다 안전하게 동작)
	void SetShapeMode(ShapeMode mode);
	void SetLocalPose(const physx::PxTransform& t);
	void SetSceneQueryEnabled(bool on);
	void SetFilterData(const physx::PxFilterData& sim, const physx::PxFilterData& query);

protected:
	// 파생 클래스가 shape 생성 후 반드시 호출
	void SetShape(physx::PxShape* shape, bool owned = true);

	void ApplyAll();

	void ApplySceneQueryFlag();


	void ApplyFilterData();


	void ApplyLocalPose();

	void ApplyShapeModeFlags();

protected:
	physx::PxShape* m_Shape = nullptr;
	bool m_Owned = true;


	ShapeMode m_Mode = ShapeMode::Simulation;
	//이 shape가 Scene Query시스템에 포함될지 정함
	//Raycast, Sweep, Overlap 등에 포함될지
	bool m_SceneQueryEnabled = false;


	//오프셋
	physx::PxTransform m_LocalPose = physx::PxTransform(physx::PxIdentity);

	//충돌 레이어 / 마스크
	physx::PxFilterData m_SimFilter{};
	physx::PxFilterData m_QueryFilter{};

};

