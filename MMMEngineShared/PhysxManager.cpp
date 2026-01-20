#include "PhysxManager.h"

void PhysxManager::BindCore(MMMEngine::PhysicX* core)
{
	m_Core = core;
}

void PhysxManager::BindScene(MMMEngine::Scene* scene)
{
	m_Scene = scene;
	m_PhysScene = scene ? &scene->GetPhysScene() : nullptr;
}

void PhysxManager::StepFixed(float dt)
{
	if (!m_PhysScene) return;

	FlushCommands_PreStep();     // 등록/부착 등
	ApplyFilterConfigIfDirty();  // dirty면 정책 갱신 + 전체 재적용 지시

	m_PhysScene->Step(dt);       // simulate/fetch + transform sync + event dispatch

	FlushCommands_PostStep();    // detach/unreg/release 등 후처리
}


// rigidbody를 physScene 시뮬레이션 대상으로 등록한다
void PhysxManager::RequestRegisterRigid(MMMEngine::RigidBodyComponent* rb)
{
	if (!rb) return;
	m_Commands.push_back({ CmdType::RegRigid, rb, nullptr });
}

// rigidbody가 가진 physX actor를 pxScene에서 제거 ( rigidbody를 더이상 물리월드에 존재하지 않게 만드는 함수 )
void PhysxManager::RequestUnregisterRigid(MMMEngine::RigidBodyComponent* rb)
{
    // 아직 처리 전인 Register/Attach/Detach 등을 정리(최소한 Reg/Attach는 제거 추천)
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->rb == rb &&
            (it->type == CmdType::RegRigid || it->type == CmdType::AttachCol))
            it = m_Commands.erase(it);
        else
            ++it;
    }
    //큐에서 지운 rigid를 unregid type으로 바꿔서 physScene에서 actor를 빼도록 함
    m_Commands.push_back({ CmdType::UnregRigid, rb, nullptr });
}

//collider를 physx로 만들고 만든 shape를 rigdbody에 attachShape함
void PhysxManager::RequestAttachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col)
{
	if (!rb || !col) return;
	m_Commands.push_back({ CmdType::AttachCol, rb, col });
}

//rigid의 actor에서 해당 collider의 pxshape를 제거함
void PhysxManager::RequestDetachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col)
{
    if (!rb || !col) return;

    // 아직 처리 전인 Attach가 있으면 상쇄
    for (auto it = m_Commands.begin(); it != m_Commands.end(); )
    {
        if (it->type == CmdType::AttachCol && it->rb == rb && it->col == col)
            it = m_Commands.erase(it);
        else
            ++it;
    }

    //attachcol했던 파일을 detach로 넣어서 없앰
    m_Commands.push_back({ CmdType::DetachCol, rb, col });
}

//collider의 shape를 다시 만들어서 원래 붙어있던 actor에 다시 붙인다 (collider쪽에 자기자신이 등록된 object확인 법필요 )
void PhysxManager::RequestRebuildCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col)
{
	if (!col) return;
    m_Commands.push_back({ CmdType::RebuildCol, rb, col });
}

//레이어/마스크 정책이 바뀌면 Scene에 존재하는 모든 shape의 filterdata를 다시 넣도록 지시
void PhysxManager::RequestReapplyFilters()
{
    m_FilterDirty = true;
}

//물리 시뮬레이션을 돌리기 직전(simulate하기전)에 큐에 쌓인 명령 중 지금 해도 안전한것을 physScene에 실행함
// actor생성 및 acotr를 추가하는 작업 / shape생성 밑 붙이는 작업 / shape 교체등을 여기서 한다
void PhysxManager::FlushCommands_PreStep()
{
    if (!m_PhysScene || !m_Core) return;

    for (auto& cmd : m_Commands)
    {
        switch (cmd.type)
        {
        case CmdType::RegRigid:
            m_PhysScene->RegisterRigid(cmd.rb, *m_Core);
            break;

        case CmdType::AttachCol:
            m_PhysScene->AttachCollider(cmd.rb, cmd.col, *m_Core, m_CollisionMatrix);
            break;

        case CmdType::RebuildCol:
            m_PhysScene->RebuildCollider(cmd.col, *m_Core, m_CollisionMatrix);
            break;

        default:
            // post에서 처리
            break;
        }
}

//물리 스탭이 완료된 후 (simulate + fetchResults가 끝난 직후 ) 큐에 쌓인 명령 중 스탭 이후에 처리하는 함수를 실행하는 함수
void PhysxManager::FlushCommands_PostStep()
{

}

//충돌 매트릭스 설정이 바뀌엇는지 확인하고 바뀌었으면 scene에 등록된 shape에 새필터를 모두 적용시키는 함수
void PhysxManager::ApplyFilterConfigIfDirty()
{
    //if (!m_FilterDirty || !m_PhysScene || !m_Scene) return;

    //// 1) Scene의 레이어/매트릭스 설정으로 CollisionMatrix 갱신
    //m_CollisionMatrix.LoadFrom(m_Scene->GetPhysicsSettings());

    //// 2) PhysScene에 “전체 콜라이더 필터 다시 넣어라” 지시
    //m_PhysScene->ReapplyFilters(m_CollisionMatrix);

    //m_FilterDirty = false;
}
