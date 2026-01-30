#include "PhysxManager.h"
#include "ColliderComponent.h"
#include "RigidBodyComponent.h"
#include "Transform.h"
#include "GameObject.h"
#include "PhysxHelper.h"
#include "rttr/registration"

RTTR_REGISTRATION
{
    using namespace rttr;
    using namespace MMMEngine;

    registration::class_<ColliderComponent>("ColliderComponent")
        (rttr::metadata("INSPECTOR", "DONT_ADD_COMP"))
        .property("StaticFriction", &ColliderComponent::GetStaticFriction, &ColliderComponent::SetStaticFriction)
        .property("DynamicFriction", &ColliderComponent::GetDynamicFriction, &ColliderComponent::SetDynamicFriction)
        .property("Restitution", &ColliderComponent::GetRestitution, &ColliderComponent::SetRestitution);
}


void MMMEngine::ColliderComponent::EnsureMaterial()
{
	auto& physics = MMMEngine::PhysicX::Get().GetPhysics();
	if (!m_Material)
	{
		m_Material = physics.createMaterial(m_StaticFriction, m_DynamicFriction, m_Restitution);
		m_MaterialOwned = true;
	}
}

void MMMEngine::ColliderComponent::ApplyMaterial()
{
	EnsureMaterial();
	if (!m_Material) return;
	m_Material->setStaticFriction(m_StaticFriction);
	m_Material->setDynamicFriction(m_DynamicFriction);
	m_Material->setRestitution(m_Restitution);
	if (m_Shape)
	{
		physx::PxMaterial* mats[1] = { m_Material };
		m_Shape->setMaterials(mats, 1);
	}
}

void MMMEngine::ColliderComponent::SetStaticFriction(float value)
{
	if (value < 0.0f) value = 0.0f;
	m_StaticFriction = value;
	ApplyMaterial();
}

void MMMEngine::ColliderComponent::SetDynamicFriction(float value)
{
	if (value < 0.0f) value = 0.0f;
	m_DynamicFriction = value;
	ApplyMaterial();
}

void MMMEngine::ColliderComponent::SetRestitution(float value)
{
	if (value < 0.0f) value = 0.0f;
	m_Restitution = value;
	ApplyMaterial();
}

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

void MMMEngine::ColliderComponent::SetRigidOffsetPose(const physx::PxTransform& pose)
{
    m_RigidOffsetPose = pose;
    ApplyLocalPose();
}

void MMMEngine::ColliderComponent::ApplyLocalPose()
{
    if (!m_Shape) return;
    m_Shape->setLocalPose(m_RigidOffsetPose * m_LocalPose);
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
    if (m_Shape)
    {
        MarkFilterDirty();
        
    }
}

void MMMEngine::ColliderComponent::SetLayer(uint32_t layer)
{
    if (m_LayerOverride == layer) return;
    m_LayerOverride = layer;
    if (m_Shape)
    {
        MarkFilterDirty();
    }
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
    PhysxManager::Get().NotifyColliderChanged(this);
}

bool MMMEngine::ColliderComponent::ApplyGeometryIfDirty()
{
    if (!m_geometryDirty)
        return false;

    if (!m_Shape)
        return false;

    const bool ok = UpdateShapeGeometry();

#ifdef _DEBUG
    if (!ok)
        OutputDebugStringA("[Collider] UpdateShapeGeometry failed.\n");
#endif

    if (ok)
    {
        m_geometryDirty = false;
        ApplyAll(); // geometry 바뀐 뒤 flags/filter/pose 재적용
    }

    return ok;
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

//void MMMEngine::ColliderComponent::SetLocalPose(const physx::PxTransform& t)
//{
//    m_LocalPose = t; ApplyAll();
//}

void MMMEngine::ColliderComponent::SetLocalCenter(Vector3 pos)
{
    m_LocalCenter = pos;
    auto pxPos = ToPxVec(m_LocalCenter);
    m_LocalPose.p = pxPos;
    ApplyLocalPose();
}

void MMMEngine::ColliderComponent::SetLocalRotation(Quaternion quater)
{
    m_LocalQuater = quater;
    auto pxQuat = ToPxQuat(m_LocalQuater);
    m_LocalPose.q = pxQuat;
    ApplyLocalPose();
}

Vector3 MMMEngine::ColliderComponent::GetLocalCenter()
{
    return m_LocalCenter;
}

Quaternion MMMEngine::ColliderComponent::GetLocalQuater()
{
    return m_LocalQuater;
}


void MMMEngine::ColliderComponent::SetSceneQueryEnabled(bool on)
{
    m_SceneQueryEnabled = on; ApplyAll();
}

void MMMEngine::ColliderComponent::SetFilterData(const physx::PxFilterData& sim, const physx::PxFilterData& query)
{
    m_SimFilter = sim; m_QueryFilter = query; ApplyAll();
}

void MMMEngine::ColliderComponent::MarkFilterDirty()
{
    m_filterDirty = true;

    // 
    PhysxManager::Get().NotifyColliderChanged(this);
}

physx::PxTransform MMMEngine::ColliderComponent::GetWorldPosPx() const
{
    if (!m_Shape) return physx::PxTransform(physx::PxIdentity);

    physx::PxRigidActor* actor = m_Shape->getActor();
    if (!actor) return physx::PxTransform(physx::PxIdentity);

    // 
    return actor->getGlobalPose() * m_Shape->getLocalPose();
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


void MMMEngine::ColliderComponent::Initialize()
{
	// 
	auto& physics = MMMEngine::PhysicX::Get().GetPhysics();
	EnsureMaterial();
	physx::PxMaterial* mat = m_Material ? m_Material : MMMEngine::PhysicX::Get().GetDefaultMaterial();

	if (mat)
	{
		BuildShape(&physics, mat);
	}
	MMMEngine::PhysxManager::Get().NotifyColliderAdded(this);
}

void MMMEngine::ColliderComponent::UnInitialize()
{
    PhysxManager::Get().NotifyColliderRemoved(this);

    if (m_Shape)
    {
        if (auto* actor = m_Shape->getActor())
            actor->detachShape(*m_Shape);

        if (m_Owned)
            m_Shape->release();

        m_Shape = nullptr;
        m_Owned = false;
    }

    if (m_Material && m_MaterialOwned)
    {
        m_Material->release();
    }
    m_Material = nullptr;
    m_MaterialOwned = false;
}

