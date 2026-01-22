#include "PhysxManager.h"
#include "SceneManager.h"
#include "GameObject.h"

DEFINE_SINGLETON(MMMEngine::SceneManager)

void MMMEngine::PhysxManager::BindScene(MMMEngine::Scene* scene)
{
	m_Scene = scene;
}

void MMMEngine::PhysxManager::StepFixed(float dt)
{
	FlushCommands_PreStep();     // 등록/부착 등
	ApplyFilterConfigIfDirty();  // dirty면 정책 갱신 + 전체 재적용 지시
    FlushDirtyColliders_PreStep(); //collider의 shape가 에디터 단계에서 변형되면 내부적으로 실행

	m_PhysScene.Step(dt);       // simulate/fetch + transform sync + event dispatch

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

    auto go = rb->GetGameObject();
    if (HasAnyCollider(go))
    {
        // 정책: collider 있으면 rigid 제거 불가
        // (여기서 로그/에디터 메시지 추천)
        return;
    }

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
    RequestUpdateCollider(col);
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

    //attachcol했던 파일을 detach로 넣어서 없앰
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

void MMMEngine::PhysxManager::RequestUpdateCollider(MMMEngine::ColliderComponent* col)
{
    if (!col) return;
    m_DirtyColliders.insert(col);
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



//물리 시뮬레이션을 돌리기 직전(simulate하기전)에 큐에 쌓인 명령 중 지금 해도 안전한것을 physScene에 실행함
// actor생성 및 acotr를 추가하는 작업 / shape생성 밑 붙이는 작업 / shape 교체등을 여기서 한다
void MMMEngine::PhysxManager::FlushCommands_PreStep()
{
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
            m_PhysScene.DetachCollider(it->new_rb, it->col);
            it = m_Commands.erase(it);
        }
        else
        {
            ++it;
        }
    }

    //Unregister
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::UnregRigid)
        {
            m_PhysScene.UnregisterRigid(it->new_rb);
            m_PendingUnreg.erase(it->new_rb);
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