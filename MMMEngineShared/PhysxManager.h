#pragma once
#include <iostream>
#include "Scene.h"
#include "TimeManager.h"
#include "PhysX.h"
#include "RigidBodyComponent.h"
#include "ColliderComponent.h"
#include "PhysScene.h"

class PhysxManager
{
public:
    // 싱글톤으로 갈 거면
    //static PhysicsManager& Get();

    // 엔진이 PhysXCore를 이미 초기화해둔 상태에서 주입받기
    void BindCore(MMMEngine::PhysicX* core);

    // 현재 씬 바인딩 (Scene이 PhysScene을 보유함)
    void BindScene(MMMEngine::Scene* scene);

    // fixed step에서 호출되는 진입점
    void StepFixed(float dt);

    // ---- 등록/해제 요청 (즉시 X, 큐잉) ----
    void RequestRegisterRigid(MMMEngine::RigidBodyComponent* rb);
    void RequestUnregisterRigid(MMMEngine::RigidBodyComponent* rb);

    void RequestAttachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col);
    void RequestDetachCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col);

    void RequestRebuildCollider(MMMEngine::RigidBodyComponent* rb, MMMEngine::ColliderComponent* col);
    void RequestReapplyFilters(); // 씬 설정 바뀌었을 때(Dirty)

private:
    // 내부에서만 쓰는 헬퍼
    void FlushCommands_PreStep();
    void FlushCommands_PostStep();

    void ApplyFilterConfigIfDirty();

private:
    // ---- 주입/바인딩 ----
    MMMEngine::PhysicX* m_Core = nullptr;             // 전역 PhysX 코어(소유 X)
    MMMEngine::Scene* m_Scene = nullptr;              // 현재 씬(소유 X)
    MMMEngine::PhysScene* m_PhysScene = nullptr;      // 현재 씬의 PhysScene 캐시(소유 X)

    // ---- 필터 정책(네가 매니저가 갖고 싶다고 했던 부분) ----
    //physx::CollisionMatrix m_CollisionMatrix;     // 레이어/마스크 정책
    bool m_FilterDirty = false;            // 에디터에서 설정 변경 시 true

    // ---- 명령 큐(중요) ----
    enum class CmdType : uint8_t
    {
        RegRigid,       //RigidBody를 physScene에 등록하는 명령 //scene->addActor(actor)
        UnregRigid,     //RigidBody를 physScene에 제거하는 명령 //scene->removeActor(actor)
        AttachCol,      //이 RigidBody에 이 Collider에 붙여라  //actor->attachShape(shape)
        DetachCol,      //이 Collider를 actor에서 뗴어라       //actor->detachShape(shape)
        RebuildCol,     //이 Collider의 shape를 다시 만들어라 //새 geometry로 BuildShape -> 다시 attach
    };

    struct Command
    {
        CmdType type;
        MMMEngine::RigidBodyComponent* rb = nullptr;
        MMMEngine::ColliderComponent* col = nullptr;
    };

    std::vector<Command> m_Commands;

};

