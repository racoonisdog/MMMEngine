#pragma once
#include <physx/PxPhysicsAPI.h>
#include "Component.h"


namespace MMMEngine
{
	class MMMENGINE_API ColliderComponent : public MMMEngine::Component
	{
	private:
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
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

		void Initialize() override;

		physx::PxShape* GetPxShape() const { return m_Shape; }
		ShapeMode GetShapeMode() const { return m_Mode; }


		// 공통 설정(Shape 생성 전/후 둘 다 안전하게 동작)
		void SetShapeMode(ShapeMode mode);
		void SetLocalPose(const physx::PxTransform& t);
		void SetSceneQueryEnabled(bool on);
		void SetFilterData(const physx::PxFilterData& sim, const physx::PxFilterData& query);


		//overlayer 설정
		void SetOverrideLayer(bool enable);
		bool IsOverrideLayer() const { return m_OverrideLayer; }

		void SetLayer(uint32_t layer);
		uint32_t GetLayer() const { return m_LayerOverride; }

		uint32_t GetEffectiveLayer();

		//shape 값 변경시 flag용 bool
		bool IsGeometryDirty() const { return m_geometryDirty; }
		void SetGeometryDirty(bool value) { m_geometryDirty = value; }
		void MarkGeometryDirty();


		//virtual bool UpdateShapeGeometry() = 0;
		/*
		if (!m_scene || !col) return;

		// attach 안 된 상태에서도 shape만 있으면 갱신은 가능.
		// attach 상태에서 갱신하는 게 일반적.
		if (!col->GetPxShape())
        return; // 정책: shape 없으면 여기서 만들지 않음(Attach/초기화에서만 생성)

		const bool ok = col->UpdateShapeGeometry();
		#ifdef _DEBUG
		if (!ok) OutputDebugStringA("[PhysScene] UpdateColliderGeometry failed.\n");
		#endif
		*/

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

		bool m_OverrideLayer = false;
		uint32_t m_LayerOverride = 0;

		bool m_geometryDirty = true;
	};
}
