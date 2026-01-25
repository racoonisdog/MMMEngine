#include "EditorCamera.h"
#include "MMMInput.h"
#include "MMMTime.h"

using namespace DirectX::SimpleMath;
using namespace DirectX;

struct FocusState {
    bool active = false;
    Vector3 targetPos;
    float duration = 0.5f; // 이동 시간
    float elapsedTime = 0.0f;
    Vector3 startPos;
};

FocusState m_focusState;

namespace CameraMathf
{
    float Lerp(float a, float b, float t)
    {
        return (a + (b - a) * t);
    }
}

void MMMEngine::Editor::EditorCamera::FocusOn(const Vector3& worldPosition, float distance)
{
    m_focusState.startPos = GetPosition();
    
    // 1. 현재 카메라가 바라보고 있는 방향 벡터를 가져옵니다.
    Matrix worldMat = GetTransformMatrix();
    Vector3 lookDir = -worldMat.Forward(); // 카메라가 보고 있는 앞방향
    
    // 2. 목표 위치 결정
    // 물체 위치(worldPosition)에서 바라보는 방향의 반대(-lookDir)로 distance만큼 이동
    m_focusState.targetPos = worldPosition - (lookDir * distance);

    m_focusState.elapsedTime = 0.0f;
    m_focusState.active = true;
}

void MMMEngine::Editor::EditorCamera::UpdateProjMatrix()
{
    XMStoreFloat4x4(&m_cachedProjMatrix, XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(m_fov), m_aspect, m_near, m_far));
}

void MMMEngine::Editor::EditorCamera::UpdateProjFrustum()
{

}

const DirectX::SimpleMath::Matrix MMMEngine::Editor::EditorCamera::GetCameraMatrix()
{
    return GetViewMatrix() * GetProjMatrix();
}

const DirectX::SimpleMath::Matrix& MMMEngine::Editor::EditorCamera::GetViewMatrix()
{
    if (m_isViewMatrixDirty)
    {
		m_cachedViewMatrix = GetTransformMatrix().Invert();
        m_isViewMatrixDirty = false;
    }

    return m_cachedViewMatrix;
}

const DirectX::SimpleMath::Matrix& MMMEngine::Editor::EditorCamera::GetProjMatrix()
{
    if (m_isProjMatrixDirty)
    {
        UpdateProjMatrix();
        UpdateProjFrustum();
        m_isProjMatrixDirty = false;
    }

    return m_cachedProjMatrix;
}

void MMMEngine::Editor::EditorCamera::MarkViewMatrixDirty()
{
    m_isViewMatrixDirty = true;
}

void MMMEngine::Editor::EditorCamera::MarkProjectionMatrixDirty()
{
    m_isProjMatrixDirty = true;
}

void MMMEngine::Editor::EditorCamera::MarkTransformMatrixDirty()
{
    m_isTransformMatrixDirty = true;
    m_isViewMatrixDirty = true;
}

void MMMEngine::Editor::EditorCamera::InputUpdate(int currentOp)
{
    const float moveSpeed = 5.0f;
    const float rotSpeed = 0.1f;
    const float panSpeed = 0.01f; // 팬 이동 속도 (조절 필요)

    static float targetPitch = DirectX::XMConvertToDegrees(m_rotation.ToEuler().x);
    static float targetYaw = DirectX::XMConvertToDegrees(m_rotation.ToEuler().y);

    static float pitch = DirectX::XMConvertToDegrees(m_rotation.ToEuler().x);
    static float yaw = DirectX::XMConvertToDegrees(m_rotation.ToEuler().y);

    auto mousePos = Input::GetMousePos();
    float deltaX = mousePos.x - m_lastMouseX;
    float deltaY = mousePos.y - m_lastMouseY;

    Vector3 targetMovement = Vector3::Zero;
    static Vector3 movement = Vector3::Zero;

    if (m_focusState.active)
    {
        m_focusState.elapsedTime += Time::GetUnscaledDeltaTime();
        float t = m_focusState.elapsedTime / m_focusState.duration;

        if (t >= 1.0f)
        {
            SetPosition(m_focusState.targetPos);
            m_focusState.active = false;

            // [중요] 포커스 완료 후 모든 보간 변수를 현재의 물리적 상태로 덮어쓰기
            auto euler = m_rotation.ToEuler();
            targetPitch = pitch = DirectX::XMConvertToDegrees(euler.x);
            targetYaw = yaw = DirectX::XMConvertToDegrees(euler.y);
            movement = Vector3::Zero;
        }
        else
        {
            t = t * t * (3.0f - 2.0f * t); // SmoothStep
            SetPosition(Vector3::Lerp(m_focusState.startPos, m_focusState.targetPos, t));

            // [중요] 포커스 도중에도 마우스 좌표는 계속 업데이트해서 delta 튐 방지
            m_lastMouseX = mousePos.x;
            m_lastMouseY = mousePos.y;
            return;
        }
    }

    if (Input::GetKey(KeyCode::MouseRight))
    {
        // 마우스 회전 처리
        if (!m_firstMouseUpdate)
        {
            if (deltaX != 0 || deltaY != 0)
            {
                // 마우스 델타로 회전 적용
                targetYaw += deltaX * rotSpeed;
                targetPitch += deltaY * rotSpeed; // Y는 반대 방향

                // Pitch 제한
                //targetPitch = std::max( -89.0f, std::min(89.0f, targetPitch));
            }
        }
        else
        {
            m_firstMouseUpdate = false;
        }

        // 이동 처리 (더 효율적으로)
        Matrix worldMat = GetTransformMatrix();

        Vector3 move =
        {
            (Input::GetKey(KeyCode::A) ? -1.0f : 0.0f) + (Input::GetKey(KeyCode::D) ? 1.0f : 0.0f),
            (Input::GetKey(KeyCode::Q) ? -1.0f : 0.0f) + (Input::GetKey(KeyCode::E) ? 1.0f : 0.0f),
            (Input::GetKey(KeyCode::S) ? -1.0f : 0.0f) + (Input::GetKey(KeyCode::W) ? 1.0f : 0.0f)
        };

        targetMovement += worldMat.Backward() * move.z;
        targetMovement += worldMat.Right() * move.x;
        targetMovement += worldMat.Up() * move.y;
    }
    else if (currentOp == 0 && Input::GetKey(KeyCode::MouseLeft))
    {
        Matrix worldMat = GetTransformMatrix();

        movement = Vector3::Zero; // 핸드 조작 시 관성 제거
        Vector3 deltaPos = (worldMat.Right() * (-deltaX * panSpeed)) + (worldMat.Up() * (deltaY * panSpeed));
        SetPosition(GetPosition() + deltaPos);
    }
    else
    {
        if (Input::GetKeyUp(KeyCode::MouseRight)) m_firstMouseUpdate = true;
    }

    pitch = CameraMathf::Lerp(pitch, targetPitch, 12.0f * Time::GetUnscaledDeltaTime());
    yaw = CameraMathf::Lerp(yaw, targetYaw, 12.0f * Time::GetUnscaledDeltaTime());
    SetEulerRotation(Vector3(pitch, yaw, 0));

    if (targetMovement.LengthSquared() > 0.0f) targetMovement.Normalize();
    if (Input::GetKey(KeyCode::LeftShift)) targetMovement *= 3.0f;

    movement = Vector3::Lerp(movement, targetMovement, 6.0f * Time::GetUnscaledDeltaTime());
    SetPosition(GetPosition() + movement * moveSpeed * Time::GetUnscaledDeltaTime());

    // 마우스 위치 업데이트 (포커스가 아닐 때도 공통 수행)
    m_lastMouseX = mousePos.x;
    m_lastMouseY = mousePos.y;
}
