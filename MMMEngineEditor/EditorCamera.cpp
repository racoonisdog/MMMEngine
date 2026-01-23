#include "EditorCamera.h"
#include "MMMInput.h"
#include "MMMTime.h"

using namespace DirectX::SimpleMath;
using namespace DirectX;

namespace CameraMathf
{
    float Lerp(float a, float b, float t)
    {
        return (a + (b - a) * t);
    }
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
}

void MMMEngine::Editor::EditorCamera::InputUpdate()
{
    const float moveSpeed = 5.0f;
    const float rotSpeed = 0.1f;

    static float targetPitch = DirectX::XMConvertToDegrees(m_rotation.ToEuler().x);
    static float targetYaw = DirectX::XMConvertToDegrees(m_rotation.ToEuler().y);

    static float pitch = DirectX::XMConvertToDegrees(m_rotation.ToEuler().x);
    static float yaw = DirectX::XMConvertToDegrees(m_rotation.ToEuler().y);

    Vector3 targetMovement = Vector3::Zero;
    static Vector3 movement = Vector3::Zero;

    auto mousePos = Input::GetMousePos();

    if (Input::GetKey(KeyCode::MouseRight))
    {
        // 마우스 회전 처리
        if (!m_firstMouseUpdate)
        {
            float deltaX = mousePos.x - m_lastMouseX;
            float deltaY = mousePos.y - m_lastMouseY;

            if (deltaX != 0 || deltaY != 0)
            {
                // 마우스 델타로 회전 적용
                targetYaw -= deltaX * rotSpeed;
                targetPitch -= deltaY * rotSpeed; // Y는 반대 방향

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
    else
    {
        // 우클릭이 해제되면 마우스 업데이트 초기화
        if (Input::GetKeyUp(KeyCode::MouseRight))
        {
            m_firstMouseUpdate = true;
        }
    }

    pitch = CameraMathf::Lerp(pitch, targetPitch, 12.0f * Time::GetUnscaledDeltaTime());
    yaw = CameraMathf::Lerp(yaw, targetYaw, 12.0f * Time::GetUnscaledDeltaTime());

    // 새 회전값 설정 (도 단위)
    SetEulerRotation(Vector3(pitch, yaw, 0));

    // 정규화 후 이동 적용
    if (targetMovement.LengthSquared() > 0.0f)
    {
        targetMovement.Normalize();
    }

    if (Input::GetKey(KeyCode::LeftShift)) targetMovement *= 3.0f;

    movement = Vector3::Lerp(movement, targetMovement, 6.0f * Time::GetUnscaledDeltaTime());

    Vector3 currentPos = GetPosition();
    SetPosition(currentPos + movement * moveSpeed * Time::GetUnscaledDeltaTime());

    // 마우스 위치 업데이트
    m_lastMouseX = mousePos.x;
    m_lastMouseY = mousePos.y;
}
