#pragma once
#define NOMINMAX
#include "SimpleMath.h"

namespace MMMEngine::Editor
{
	class EditorCamera
	{
	private:
		DirectX::SimpleMath::Vector3 m_position;
		DirectX::SimpleMath::Quaternion m_rotation;
		mutable DirectX::SimpleMath::Matrix m_cachedTransformMatrix;

		mutable DirectX::BoundingFrustum m_cachedProjFrustum;

		float m_fov;
		float m_near;
		float m_far;
		float m_aspect;
		bool m_isOrthographic = false;
		float m_orthoSize = 10.0f;
		float m_projectionBlend = 0.0f;
		float m_projectionBlendSpeed = 6.0f;

		mutable bool m_isProjMatrixDirty;
		mutable bool m_isViewMatrixDirty;
		mutable bool m_isTransformMatrixDirty;

		mutable DirectX::SimpleMath::Matrix m_cachedViewMatrix; // view Matrix
		mutable DirectX::SimpleMath::Matrix m_cachedProjMatrix; // projection Matrix

		int m_lastMouseX = 0;
		int m_lastMouseY = 0;
		bool m_firstMouseUpdate = true;
		float m_targetPitch = 0.0f;
		float m_targetYaw = 0.0f;
		float m_pitch = 0.0f;
		float m_yaw = 0.0f;
		bool m_inputStateDirty = true;
		DirectX::SimpleMath::Vector3 m_smoothedMovement = DirectX::SimpleMath::Vector3::Zero;

		void UpdateProjMatrix();
		void UpdateProjFrustum();
	public:
		const DirectX::SimpleMath::Matrix GetCameraMatrix();  // view * projection Matrix
		const DirectX::SimpleMath::Matrix& GetViewMatrix();
		const DirectX::SimpleMath::Matrix& GetProjMatrix();

		const DirectX::BoundingFrustum& GetProjFrustum();

		const DirectX::SimpleMath::Matrix& GetTransformMatrix()
		{
			if (m_isTransformMatrixDirty)
			{
				m_isTransformMatrixDirty = false;
				m_cachedTransformMatrix =
					DirectX::SimpleMath::Matrix::CreateScale({ 1.0f,1.0f,1.0f }) *
					DirectX::SimpleMath::Matrix::CreateFromQuaternion(m_rotation) *
					DirectX::SimpleMath::Matrix::CreateTranslation(m_position);
			}
			return m_cachedTransformMatrix;
		}


		inline const float& GetFOV() const { return m_fov; }
		inline const float& GetNearPlane() const { return m_near; }
		inline const float& GetFarPlane() const { return m_far; }
		inline const float& GetAspectRatio() const { return m_aspect; }
		inline bool IsOrthographic() const { return m_projectionBlend >= 0.5f; }
		inline bool IsOrthographicTarget() const { return m_isOrthographic; }
		inline float GetOrthoSize() const { return m_orthoSize; }
		inline float GetProjectionBlend() const { return m_projectionBlend; }

		inline const DirectX::SimpleMath::Vector3& GetPosition() const { return m_position; }
		inline const DirectX::SimpleMath::Quaternion& GetRotation() const { return m_rotation; }

		inline void SetPosition(DirectX::SimpleMath::Vector3 pos) { m_position = pos; MarkTransformMatrixDirty(); }
		inline void SetPosition(float x, float y, float z) { m_position = { x,y,z }; MarkTransformMatrixDirty(); }

		inline void SetRotation(DirectX::SimpleMath::Quaternion rot) { m_rotation = rot; MarkTransformMatrixDirty(); }
		inline void SetEulerRotation(DirectX::SimpleMath::Vector3 rot) 
		{ 	
			// 도 단위를 라디안으로 변환
			auto radRotX = DirectX::XMConvertToRadians(rot.x);
			auto radRotY = DirectX::XMConvertToRadians(rot.y);
			auto radRotZ = DirectX::XMConvertToRadians(rot.z);

			// DirectX 방식으로 로컬 회전 쿼터니언 생성
			m_rotation = DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(radRotY, radRotX, radRotZ);

			MarkTransformMatrixDirty();
		}

		inline void SetFOV(float value) { m_fov = value; MarkProjectionMatrixDirty(); }
		inline void SetNearPlane(float value) { m_near = value; MarkProjectionMatrixDirty(); }
		inline void SetFarPlane(float value) { m_far = m_near + 0.01f > value ? m_near + 0.01f : value; MarkProjectionMatrixDirty(); }
		inline void SetAspectRatio(float value) { m_aspect = value; MarkProjectionMatrixDirty(); }
		inline void SetAspectRatio(float width, float height) { m_aspect = width / height; MarkProjectionMatrixDirty(); }
		inline void SetOrthographic(bool value) { m_isOrthographic = value; MarkProjectionMatrixDirty(); }
		inline void SetOrthoSize(float value) { m_orthoSize = value < 0.01f ? 0.01f : value; MarkProjectionMatrixDirty(); }
		inline void ToggleProjectionMode() { SetOrthographic(!m_isOrthographic); }
		inline void SyncInputState() { m_inputStateDirty = true; }

		void MarkViewMatrixDirty();
		void MarkProjectionMatrixDirty();
		void MarkTransformMatrixDirty();

		inline const bool IsDirtyMatrix() { return m_isProjMatrixDirty || m_isViewMatrixDirty || m_isTransformMatrixDirty; }

		void FocusOn(const DirectX::SimpleMath::Vector3& worldPosition, float distance = 5.0f);
		void UpdateProjectionBlend();
		void InputUpdate(int currentOp);
	};
}
