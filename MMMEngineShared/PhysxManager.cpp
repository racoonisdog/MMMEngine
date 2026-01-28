#include "PhysxManager.h"
#include "SceneManager.h"
#include "GameObject.h"
#include "BehaviourManager.h"

DEFINE_SINGLETON(MMMEngine::PhysxManager)

void MMMEngine::PhysxManager::BindScene(MMMEngine::Scene* scene)
{
    if (m_Scene == scene) return;

    UnbindScene();

    m_Scene = scene;
    if (!m_Scene) return;

    // 씬 설정으로 desc 구성 (임시라도)
    PhysSceneDesc desc{};

    for (uint32_t i = 0; i <= 4; ++i)
    {
        for (uint32_t j = 0; j <= 4; ++j)
        {
            m_CollisionMatrix.SetCanCollide(i, j, true);
        }
    }

    if (m_PhysScene.Create(desc))
    {
        m_IsInitialized = true;
        m_FilterDirty = true;
    }
    else
    {
        std::cout << "Scene create failed" << std::endl;
        m_Scene = nullptr;
    }
}

void MMMEngine::PhysxManager::SetStep()
{
    if (!m_IsInitialized) return;
    FlushCommands_PreStep();     // 등록/부착 등
    ApplyFilterConfigIfDirty();  // dirty면 정책 갱신 + 전체 재적용 지시
    FlushDirtyColliders_PreStep(); //collider의 shape가 에디터 단계에서 변형되면 내부적으로 실행
}

void MMMEngine::PhysxManager::StepFixed(float dt)
{
    if (!m_IsInitialized) return;
    if (dt <= 0.f) return;

    m_PhysScene.PushRigidsToPhysics(); //등록된 rb목록을 순회하면서 pushtoPhysics를 호출
    m_PhysScene.Step(dt);       // simulate/fetch
    m_PhysScene.PullRigidsFromPhysics();   // PhysX->엔진 읽기 (pose)
    m_PhysScene.DrainEvents();             // 이벤트 drain

    DispatchPhysicsEvents();

    FlushCommands_PostStep();    // detach/unreg/release 등 후처리
}

void MMMEngine::PhysxManager::NotifyRigidAdded(RigidBodyComponent* rb)
{
    if (!rb) return;
    RequestRegisterRigid(rb);

}

void MMMEngine::PhysxManager::NotifyRigidRemoved(RigidBodyComponent* rb)
{
    if (!rb) return;
    //auto go = rb->GetGameObject();
    //if (!go.IsValid()) return;
    ////중복 rigid 생성 후 즉시 destroy되는 케이스 방어 (선택)
    ////현재 GO의 대표 rigid가 rb가 아니라면, 이 rb는 물리월드에 등록됐을 가능성이 낮음.
    ////그래도 안전하게 Unregister만 요청(혹시 등록돼있으면 제거되게).
    //auto currentRbPtr = go->GetComponent<RigidBodyComponent>();
    //RigidBodyComponent* currentRb = currentRbPtr.IsValid()
    //    ? static_cast<RigidBodyComponent*>(currentRbPtr.GetRaw())
    //    : nullptr;

    //if (currentRb && currentRb != rb)
    //{
    //    // "대표 rigid"가 아닌 중복 rigid 제거: collider 건드릴 필요 없음
    //    RequestUnregisterRigid(rb);
    //    return;
    //}

    ////마지막으로 rigid 제거(PhysX unregister) 예약
    RequestUnregisterRigid(rb);
}

void MMMEngine::PhysxManager::NotifyColliderAdded(ColliderComponent* col)
{
    if (!col) return;

    auto go = col->GetGameObject();
    if (!go.IsValid()) return;

    auto rbPtr = go->GetComponent<RigidBodyComponent>();
    RigidBodyComponent* rb = rbPtr.IsValid() ? (RigidBodyComponent*)rbPtr.GetRaw()
        : GetOrCreateRigid(go);

    if (!rb) return;

    RequestRegisterRigid(rb);
    RequestAttachCollider(rb, col);
}

void MMMEngine::PhysxManager::NotifyColliderRemoved(ColliderComponent* col)
{
    if (!col) return;

    auto go = col->GetGameObject();
    if (!go.IsValid()) return;

    auto rbPtr = go->GetComponent<RigidBodyComponent>();
    if (!rbPtr.IsValid()) return; // 정책상 거의 없어야 하지만 방어
    auto* rb = (RigidBodyComponent*)rbPtr.GetRaw();

    RequestDetachCollider(rb, col);
}

void MMMEngine::PhysxManager::NotifyColliderChanged(ColliderComponent* col)
{
    if (!col) return;

    if (col->IsGeometryDirty())
        m_DirtyColliders.insert(col);

    if (col->IsFilterDirty())
        m_FilterDirtyColliders.insert(col);
}


// rigidbody를 physScene 시뮬레이션 대상으로 등록한다
void MMMEngine::PhysxManager::RequestRegisterRigid(MMMEngine::RigidBodyComponent* rb)
{
    if (!rb) return;
    if (m_PendingUnreg.find(rb) != m_PendingUnreg.end()) return;

    // rb 관련 RegRigid 중복 제거 (혹은 rb 관련 명령 정리 정책에 맞게)
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::RegRigid && it->new_rb == rb)
            it = m_Commands.erase(it);
        else
            ++it;
    }

    m_Commands.push_back({ CmdType::RegRigid, rb, nullptr });
}

// rigidbody가 가진 physX actor를 pxScene에서 제거 ( rigidbody를 더이상 물리월드에 존재하지 않게 만드는 함수 )
void MMMEngine::PhysxManager::RequestUnregisterRigid(MMMEngine::RigidBodyComponent* rb)
{
    if (!rb) return;

    //예약된거면 중복방지용
    if (m_PendingUnreg.find(rb) != m_PendingUnreg.end()) return;

    // 아직 처리 전인 Register/Attach/Detach 등을 정리(최소한 Reg/Attach는 제거 추천)
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->new_rb == rb)
            it = m_Commands.erase(it);
        else
            ++it;
    }
    //지울예정인 큐에 담음
    m_PendingUnreg.insert(rb);
    //큐에서 지운 rigid를 unregid type으로 바꿔서 physScene에서 actor를 빼도록 함
    m_Commands.push_back({ CmdType::UnregRigid, rb, nullptr });
}

//collider를 physx로 만들고 만든 shape를 rigdbody에 attachShape함
void MMMEngine::PhysxManager::RequestAttachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col)
{
    if (!rb || !col) return;
    if (m_PendingUnreg.find(rb) != m_PendingUnreg.end())
    {
        return;
    }

    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        //DetachCol이 예약되어있다면 상쇄함
        if (it->type == CmdType::DetachCol && it->new_rb == rb && it->col == col)
            it = m_Commands.erase(it);
        else
            ++it;
    }
    m_Commands.push_back({ CmdType::AttachCol, rb, col });
}

//rigid의 actor에서 해당 collider의 pxshape를 제거함
void MMMEngine::PhysxManager::RequestDetachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col)
{
    if (!rb || !col) return;

    if (m_PendingUnreg.find(rb) != m_PendingUnreg.end()) return;
    // 아직 처리 전인 Attach가 있으면 상쇄
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::AttachCol && it->new_rb == rb && it->col == col)
            it = m_Commands.erase(it);
        else
            ++it;
    }

    //attachcol했던 파일을 detach로 넣어서 제거
    m_Commands.push_back({ CmdType::DetachCol, rb, col });
}

//collider의 shape를 다시 만들어서 원래 붙어있던 actor에 다시 붙인다 (collider쪽에 자기자신이 등록된 object확인 법필요 )
void MMMEngine::PhysxManager::RequestRebuildCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col)
{
    if (!col) return;

    //같은 col에 rebuil가 이미 있으면 중복 제거
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::RebuildCol && it->col == col)
            it = m_Commands.erase(it);
        else
            ++it;
    }
    m_Commands.push_back({ CmdType::RebuildCol, rb, col });
}

//레이어/마스크 정책이 바뀌면 Scene에 존재하는 모든 shape의 filterdata를 다시 넣도록 지시
void MMMEngine::PhysxManager::RequestReapplyFilters()
{
    m_FilterDirty = true;
}

void MMMEngine::PhysxManager::RequestChangeRigidType(MMMEngine::RigidBodyComponent* rb)
{
    if (!rb) return;

    // Unregister 예정이면 타입 바꿀 의미가 없음 (어차피 사라짐)
    if (m_PendingUnreg.find(rb) != m_PendingUnreg.end())
        return;

    //같은 rb에 대한 이전 ChangeRigidType 요청이 있으면 제거 (마지막 요청만 남김)
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::ChangeRigid && it->new_rb == rb)
            it = m_Commands.erase(it);
        else
            ++it;
    }

    // 정책: 타입 변경은 "actor 재생성"이라, 기존 Attach/Detach이 뒤섞이면 위험
    //    - 가장 안전한 정책은: 타입 변경 요청 시점에 rb 관련 Attach/Detach을 정리하거나
    //    - 혹은 Flush 순서를 ChangeRigidType -> Attach/Detach로 강제하는 것
    //
    // 여기서는 "Flush 순서 강제"로 가는 게 보통 더 낫다.
    // 따라서 여기서는 지우지 않고, FlushCommands_PreStep에서 ChangeRigidType을 먼저 처리하게 만든다.

    m_Commands.push_back({ CmdType::ChangeRigid, rb, nullptr });
}

MMMEngine::RigidBodyComponent* MMMEngine::PhysxManager::GetOrCreateRigid(ObjPtr<GameObject> go)
{
    if (!go.IsValid()) return nullptr;

    go->AddComponent<RigidBodyComponent>();
    auto auto_rb = go->GetComponent<RigidBodyComponent>();
    return auto_rb.IsValid() ? static_cast<RigidBodyComponent*>(auto_rb.GetRaw()) : nullptr;
}

bool MMMEngine::PhysxManager::HasAnyCollider(ObjPtr<GameObject> go) const
{
    if (!go.IsValid()) return false;

    return (go->GetComponent<ColliderComponent>().IsValid());
}


void MMMEngine::PhysxManager::SetSceneGravity(float x, float y, float z)
{
    m_PhysScene.SetGravity(x, y, z);
}

void MMMEngine::PhysxManager::SetLayerCollision(uint32_t layerA, uint32_t layerB, bool canCollide)
{
    m_CollisionMatrix.SetCanCollide(layerA, layerB, canCollide);
    m_FilterDirty = true;
}

//물리 시뮬레이션을 돌리기 직전(simulate하기전)에 큐에 쌓인 명령 중 지금 해도 안전한것을 physScene에 실행함
// actor생성 및 acotr를 추가하는 작업 / shape생성 밑 붙이는 작업 / shape 교체등을 여기서 한다
void MMMEngine::PhysxManager::FlushCommands_PreStep()
{
    //ChangeRigidType 먼저 처리하도록
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::ChangeRigid)
        {
            m_PhysScene.ChangeRigidType(it->new_rb, m_CollisionMatrix);
            it = m_Commands.erase(it);
        }
        else ++it;
    }


    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        switch (it->type)
        {
        case CmdType::RegRigid:
            m_PhysScene.RegisterRigid(it->new_rb);
            it = m_Commands.erase(it);
            break;

        case CmdType::AttachCol:
            m_PhysScene.AttachCollider(it->new_rb, it->col, m_CollisionMatrix);
            it = m_Commands.erase(it);
            break;

        case CmdType::RebuildCol:
            m_PhysScene.RebuildCollider(it->col, m_CollisionMatrix);
            it = m_Commands.erase(it);
            break;
        default:
            // Post에서 처리할 타입(Detach/Unreg)은 남겨둔다
            ++it;
            break;
        }
    }
}


//물리 스탭이 완료된 후 (simulate + fetchResults가 끝난 직후 ) 큐에 쌓인 명령 중 스탭 이후에 처리하는 함수를 실행하는 함수
void MMMEngine::PhysxManager::FlushCommands_PostStep()
{
    //Detach
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::DetachCol)
        {
            auto* rb = it->new_rb;
            auto* col = it->col;

            // rb가 곧 Unregister 될 예정이면 Detach는 의미 없거나 위험할 수 있음
            if (!rb || m_PendingUnreg.find(rb) != m_PendingUnreg.end())
            {
                it = m_Commands.erase(it);
                continue;
            }

            // actor가 이미 없으면 detach할 것도 없음 (안전)
            if (rb->GetPxActor() == nullptr)
            {
                it = m_Commands.erase(it);
                continue;
            }

            m_PhysScene.DetachCollider(rb, col);
            it = m_Commands.erase(it);
        }
        else
        {
            ++it;
        }
    }

    // Unregister
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::UnregRigid)
        {
            auto* rb = it->new_rb;

            if (rb)
            {
                m_PhysScene.UnregisterRigid(rb); // 내부에서 actor 존재 체크 + destroy idempotent면 안정
                m_PendingUnreg.erase(rb);
            }

            it = m_Commands.erase(it);
        }
        else
        {
            ++it;
        }
    }

}

//충돌 매트릭스 설정이 바뀌엇는지 확인하고 바뀌었으면 scene에 등록된 shape에 새필터를 모두 적용시키는 함수
void MMMEngine::PhysxManager::ApplyFilterConfigIfDirty()
{
    if (!m_FilterDirty) return;
    if (!m_Scene) return;

    //최신 설정으로 갱신
    //Todo Matrix에 설정된 load함수 구할수 있도록 또는 현재 Scene에서 설정가져올수있도록
    //m_CollisionMatrix.LoadFrom(m_Scene->GetPhysicsSettings());

    //현재 씬의 모든 attached collider에 재적용
    m_PhysScene.ReapplyFilters(m_CollisionMatrix);

    m_FilterDirty = false;
}

void MMMEngine::PhysxManager::FlushDirtyColliders_PreStep()
{
    if (m_DirtyColliders.empty()) return;

    for (auto* col : m_DirtyColliders)
    {
        if (!col) continue;
        // PhysScene이 ownerByCollider로 rb 찾게 할 예정
        m_PhysScene.UpdateColliderGeometry(col);
    }
    m_DirtyColliders.clear();
}

void MMMEngine::PhysxManager::FlushDirtyColliderFilters_PreStep()
{
    if (m_FilterDirtyColliders.empty()) return;

    for (auto* col : m_FilterDirtyColliders)
    {
        if (!col) continue;
        if (!col->GetPxShape()) { col->ClearFilterDirty(); continue; } // 아직 생성 전이면 스킵

        const uint32_t layer = col->GetEffectiveLayer();
        col->SetFilterData(m_CollisionMatrix.MakeSimFilter(layer),
            m_CollisionMatrix.MakeQueryFilter(layer));

        // 필터 데이터 변경을 PhysX pair에 반영
        m_PhysScene.ResetFilteringFor(col);

        col->ClearFilterDirty();
    }
    m_FilterDirtyColliders.clear();
}



void MMMEngine::PhysxManager::EraseCommandsForRigid(MMMEngine::RigidBodyComponent* rb)
{
    if (!rb) return;
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->new_rb == rb) it = m_Commands.erase(it);
        else ++it;
    }
}

void MMMEngine::PhysxManager::EraseAttachDetachForRigid(MMMEngine::RigidBodyComponent* rb)
{
    if (!rb) return;

    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if ((it->new_rb == rb) &&
            (it->type == CmdType::AttachCol || it->type == CmdType::DetachCol))
            it = m_Commands.erase(it);
        else
            ++it;
    }
}

void MMMEngine::PhysxManager::EraseCommandsForCollider(MMMEngine::ColliderComponent* col)
{
    if (!col) return;

    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->col == col &&
            (it->type == CmdType::AttachCol || it->type == CmdType::DetachCol || it->type == CmdType::RebuildCol))
            it = m_Commands.erase(it);
        else
            ++it;
    }
}

void MMMEngine::PhysxManager::NotifyRigidTypeChanged(RigidBodyComponent* rb)
{
    if (!rb) return;
    RequestChangeRigidType(rb); // 내부 커맨드 큐 적재
}

void MMMEngine::PhysxManager::UnbindScene()
{
    m_Commands.clear();
    m_DirtyColliders.clear();
    m_PendingUnreg.clear();
    m_FilterDirty = false;

    if (m_IsInitialized)
    {
        m_PhysScene.Destroy();
        m_IsInitialized = false;
    }

    m_Scene = nullptr;

    m_PhysScene.Destroy();
}

void MMMEngine::PhysxManager::DispatchPhysicsEvents()
{
    // 1) Contact (충돌)
    const auto& contacts = m_PhysScene.GetFrameContacts();
    for (const auto& e : contacts)
    {
        // userData -> 엔진 컴포넌트 복구
        auto* rbA = static_cast<RigidBodyComponent*>(e.a ? e.a->userData : nullptr);
        auto* rbB = static_cast<RigidBodyComponent*>(e.b ? e.b->userData : nullptr);
        auto* colA = static_cast<ColliderComponent*>(e.aShape ? e.aShape->userData : nullptr);
        auto* colB = static_cast<ColliderComponent*>(e.bShape ? e.bShape->userData : nullptr);

        if (!rbA || !rbB || !colA || !colB) continue;

        auto goA = rbA->GetGameObject();
        auto goB = rbB->GetGameObject();
        if (!goA.IsValid() || !goB.IsValid()) continue;

        const bool enter = (e.events & physx::PxPairFlag::eNOTIFY_TOUCH_FOUND) != 0;
        const bool stay = (e.events & physx::PxPairFlag::eNOTIFY_TOUCH_PERSISTS) != 0;
        const bool exit = (e.events & physx::PxPairFlag::eNOTIFY_TOUCH_LOST) != 0;


        if (enter) Callback_Que.push_back({ goA, goB, P_EvenType::C_enter });
        if (stay)  Callback_Que.push_back({ goA, goB, P_EvenType::C_stay });
        if (exit)  Callback_Que.push_back({ goA, goB, P_EvenType::C_out });
    }

    // 2) Trigger
    const auto& triggers = m_PhysScene.GetFrameTriggers();
    for (const auto& t : triggers)
    {
        auto* triggerCol = static_cast<ColliderComponent*>(t.triggerShape ? t.triggerShape->userData : nullptr);
        auto* otherCol = static_cast<ColliderComponent*>(t.otherShape ? t.otherShape->userData : nullptr);
        if (!triggerCol || !otherCol) continue;

        auto goT = triggerCol->GetGameObject();
        auto goO = otherCol->GetGameObject();
        if (!goT.IsValid() || !goO.IsValid()) continue;

        if (t.isEnter)
        {
            Callback_Que.push_back({ goT, goO, P_EvenType::T_enter });

        }
        else
        {
            Callback_Que.push_back({ goT, goO, P_EvenType::T_out });
        }
    }
}


void MMMEngine::PhysxManager::Shutdown()
{
    UnbindScene();
}
