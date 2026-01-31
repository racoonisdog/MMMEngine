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
    const float orthoHeight = m_orthoSize;
    const float orthoWidth = orthoHeight * m_aspect;
    const float fovRadians = DirectX::XMConvertToRadians(m_fov);

    Matrix persp;
    Matrix ortho;
    XMStoreFloat4x4(&persp, XMMatrixPerspectiveFovLH(fovRadians, m_aspect, m_near, m_far));
    XMStoreFloat4x4(&ortho, XMMatrixOrthographicLH(orthoWidth, orthoHeight, m_near, m_far));

    if (m_projectionBlend <= 0.0f)
    {
        m_cachedProjMatrix = persp;
    }
    else if (m_projectionBlend >= 1.0f)
    {
        m_cachedProjMatrix = ortho;
    }
    else
    {
        m_cachedProjMatrix = Matrix::Lerp(persp, ortho, m_projectionBlend);
    }
}

void MMMEngine::Editor::EditorCamera::UpdateProjFrustum()
{
    BoundingFrustum::CreateFromMatrix(m_cachedProjFrustum, m_cachedProjMatrix);
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

const DirectX::BoundingFrustum& MMMEngine::Editor::EditorCamera::GetProjFrustum()
{
    if (m_isProjMatrixDirty)
    {
        UpdateProjMatrix();
        UpdateProjFrustum();
        m_isProjMatrixDirty = false;
    }

    return m_cachedProjFrustum;
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

void MMMEngine::Editor::EditorCamera::UpdateProjectionBlend()
{
    float target = m_isOrthographic ? 1.0f : 0.0f;
    float delta = target - m_projectionBlend;

    if (delta > 0.0001f || delta < -0.0001f)
    {
        float step = m_projectionBlendSpeed * Time::GetUnscaledDeltaTime();
        if (m_projectionBlend < target)
        {
            m_projectionBlend += step;
            if (m_projectionBlend > target)
                m_projectionBlend = target;
        }
        else
        {
            m_projectionBlend -= step;
            if (m_projectionBlend < target)
                m_projectionBlend = target;
        }
        MarkProjectionMatrixDirty();
    }
    else if (m_projectionBlend != target)
    {
        m_projectionBlend = target;
        MarkProjectionMatrixDirty();
    }
}

void MMMEngine::Editor::EditorCamera::InputUpdate(int currentOp)
{
    const float moveSpeed = 5.0f;
    const float rotSpeed = 0.1f;
    const float panSpeed = 0.01f; // 팬 이동 속도 (조절 필요)

    if (m_inputStateDirty)
    {
        auto euler = m_rotation.ToEuler();
        m_targetPitch = m_pitch = DirectX::XMConvertToDegrees(euler.x);
        m_targetYaw = m_yaw = DirectX::XMConvertToDegrees(euler.y);
        m_smoothedMovement = Vector3::Zero;
        m_inputStateDirty = false;
    }

    auto mousePos = Input::GetMousePos();
    float deltaX = mousePos.x - m_lastMouseX;
    float deltaY = mousePos.y - m_lastMouseY;

    Vector3 targetMovement = Vector3::Zero;

    float wheel = Input::GetMouseScrollNotches();

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
            m_targetPitch = m_pitch = DirectX::XMConvertToDegrees(euler.x);
            m_targetYaw = m_yaw = DirectX::XMConvertToDegrees(euler.y);
            m_smoothedMovement = Vector3::Zero;
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
                m_targetYaw += deltaX * rotSpeed;
                m_targetPitch += deltaY * rotSpeed; // Y는 반대 방향

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
    else if (wheel != 0.0f)
    {
        // 관성 제거: 휠로 움직일 때는 기존 movement 스무딩을 끊어줌
        m_smoothedMovement = Vector3::Zero;
        targetMovement = Vector3::Zero;

        if (m_isOrthographic)
        {
            const float zoomSpeed = 1.0f; // 조절값(노치당 크기 변화)
            SetOrthoSize(m_orthoSize - (wheel * zoomSpeed));
        }
        else
        {
            // 현재 카메라의 진행방향(앞/뒤)로 이동
            Matrix worldMat = GetTransformMatrix();
            Vector3 forward = worldMat.Backward(); // 카메라가 바라보는 방향

            // 스크롤 방향/속도: 필요하면 부호만 바꿔
            const float zoomSpeed = 2.0f; // 조절값(노치당 몇 유닛 이동)
            SetPosition(GetPosition() + forward * (wheel * zoomSpeed));
        }
    }
    else if (currentOp == 0 && Input::GetKey(KeyCode::MouseLeft))
    {
        Matrix worldMat = GetTransformMatrix();

        m_smoothedMovement = Vector3::Zero; // 핸드 조작 시 관성 제거
        Vector3 deltaPos = (worldMat.Right() * (-deltaX * panSpeed)) + (worldMat.Up() * (deltaY * panSpeed));
        SetPosition(GetPosition() + deltaPos);
    }
    else
    {
        if (Input::GetKeyUp(KeyCode::MouseRight)) m_firstMouseUpdate = true;
    }

    m_pitch = CameraMathf::Lerp(m_pitch, m_targetPitch, 12.0f * Time::GetUnscaledDeltaTime());
    m_yaw = CameraMathf::Lerp(m_yaw, m_targetYaw, 12.0f * Time::GetUnscaledDeltaTime());
    SetEulerRotation(Vector3(m_pitch, m_yaw, 0));

    if (targetMovement.LengthSquared() > 0.0f) targetMovement.Normalize();
    if (Input::GetKey(KeyCode::LeftShift)) targetMovement *= 3.0f;

    m_smoothedMovement = Vector3::Lerp(m_smoothedMovement, targetMovement, 6.0f * Time::GetUnscaledDeltaTime());
    SetPosition(GetPosition() + m_smoothedMovement * moveSpeed * Time::GetUnscaledDeltaTime());

    // 마우스 위치 업데이트 (포커스가 아닐 때도 공통 수행)
    m_lastMouseX = mousePos.x;
    m_lastMouseY = mousePos.y;
}
