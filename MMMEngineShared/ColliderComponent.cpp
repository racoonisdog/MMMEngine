#include "PhysxManager.h"
#include "ColliderComponent.h"
#include "RigidBodyComponent.h"
#include "Transform.h"
#include "GameObject.h"




void MMMEngine::ColliderComponent::ApplySceneQueryFlag()
{
    if (!m_Shape) return;

    bool query = m_SceneQueryEnabled;

    if (m_Mode == ShapeMode::Disabled) query = false;
    if (m_Mode == ShapeMode::QueryOnly) query = true;

    m_Shape->setFlag(physx::PxShapeFlag::eSCENE_QUERY_SHAPE, query);
}

//*** 레이어 규칙 변경했는데 즉시 반영이 안된다면 여기를 건드리기
void MMMEngine::ColliderComponent::ApplyFilterData()
{
    if (!m_Shape) return;
    m_Shape->setSimulationFilterData(m_SimFilter);
    m_Shape->setQueryFilterData(m_QueryFilter);
}

void MMMEngine::ColliderComponent::ApplyLocalPose()
{
    if (!m_Shape) return;
    m_Shape->setLocalPose(m_LocalPose);
}

void MMMEngine::ColliderComponent::ApplyShapeModeFlags()
{
    if (!m_Shape) return;

    switch (m_Mode)
    {
    case ShapeMode::Simulation:
        m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, true);
        m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
        break;

    case ShapeMode::Trigger:
        m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
        m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, true);
        break;

    case ShapeMode::QueryOnly:
        m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
        m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
        break;

    case ShapeMode::Disabled:
        m_Shape->setFlag(physx::PxShapeFlag::eSIMULATION_SHAPE, false);
        m_Shape->setFlag(physx::PxShapeFlag::eTRIGGER_SHAPE, false);
        break;
    }
}

void MMMEngine::ColliderComponent::SetOverrideLayer(bool enable)
{
    m_OverrideLayer = enable;
}

void MMMEngine::ColliderComponent::SetLayer(uint32_t layer)
{
    m_LayerOverride = layer;
}

uint32_t MMMEngine::ColliderComponent::GetEffectiveLayer()
{
    if (m_OverrideLayer) return m_LayerOverride;
    else
    {
        if(GetGameObject().IsValid())
		{
			const auto Obj_layer = GetGameObject()->GetLayer();
			return Obj_layer;
		}
        else
        {
            return 0;
        }
    }  
}

void MMMEngine::ColliderComponent::MarkGeometryDirty()
{
    m_geometryDirty = true;
    //Todo Notifiycolliderchanges 로 전달 예정
    PhysxManager::Get().NotifyColliderChanged(this);
}


//Trigger가 Scene Query에서 기본적으로 빠지는 정책인 상태 아니면 여기 수정
void MMMEngine::ColliderComponent::SetShapeMode(ShapeMode mode)
{
    m_Mode = mode;

    switch (m_Mode)
    {
    case ShapeMode::Simulation: m_SceneQueryEnabled = true;  break;
    case ShapeMode::Trigger:    m_SceneQueryEnabled = false; break;
    case ShapeMode::QueryOnly:  m_SceneQueryEnabled = true;  break;
    case ShapeMode::Disabled:   m_SceneQueryEnabled = false; break;
    }

    ApplyAll();
}

void MMMEngine::ColliderComponent::SetLocalPose(const physx::PxTransform& t)
{
    m_LocalPose = t; ApplyAll();
}

void MMMEngine::ColliderComponent::SetSceneQueryEnabled(bool on)
{
    m_SceneQueryEnabled = on; ApplyAll();
}

void MMMEngine::ColliderComponent::SetFilterData(const physx::PxFilterData& sim, const physx::PxFilterData& query)
{
    m_SimFilter = sim; m_QueryFilter = query; ApplyAll();
}

void MMMEngine::ColliderComponent::SetShape(physx::PxShape* shape, bool owned)
{
    if (m_Shape)
    {
        if (physx::PxRigidActor* actor = m_Shape->getActor())
            actor->detachShape(*m_Shape);

        if (m_Owned)
            m_Shape->release();
    }
    //if (m_Shape && m_Owned) m_Shape->release();

    m_Shape = shape;
    m_Owned = owned;

    if (m_Shape)
    {
        m_Shape->userData = this; // PhysScene 이벤트 매핑용(핵심)
        ApplyAll();
    }
}

void MMMEngine::ColliderComponent::ApplyAll()
{
    ApplyShapeModeFlags();
    ApplySceneQueryFlag();
    ApplyFilterData();
    ApplyLocalPose();
}
