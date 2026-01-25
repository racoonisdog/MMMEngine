#pragma once
#include "Export.h"
#include "Behaviour.h"
#include "SimpleMath.h"

namespace MMMEngine
{
	class MMMENGINE_API Camera : public Behaviour
	{
	private:
		RTTR_ENABLE(Behaviour)
		RTTR_REGISTRATION_FRIEND
		static ObjPtr<Camera> s_mainCam;

		float m_fov; // 0 ~ 360 degree
		float m_near; // near plane
		float m_far; // far plane
		float m_aspect; // { width / height } ratio
		mutable bool m_isProjMatrixDirty;
		mutable bool m_isViewMatrixDirty;

		mutable DirectX::SimpleMath::Matrix m_cachedViewMatrix; // view Matrix
		mutable DirectX::SimpleMath::Matrix m_cachedProjMatrix; // projection Matrix

		void UpdateProjMatrix();
	public:
		void MarkViewMatrixDirty();
		void MarkProjectionMatrixDirty();

		const DirectX::SimpleMath::Matrix GetCameraMatrix();
		const DirectX::SimpleMath::Matrix& GetViewMatrix();
		const DirectX::SimpleMath::Matrix& GetProjMatrix();

		virtual void Initialize() override;
		virtual void UnInitialize() override;

		const float& GetFov() noexcept;
		const float& GetNear() noexcept;
		const float& GetFar() noexcept;
		const float& GetAsepct() noexcept;

		void SetFOV(const float& value);
		void SetNear(const float& value);
		void SetFar(const float& value);
		void SetAspect(const float& value);

		static ObjPtr<Camera> GetMainCamera();
		static ObjPtr<GameObject> CreateMainCamera();

		inline const bool IsDirtyMatrix() { return m_isProjMatrixDirty || m_isViewMatrixDirty; }
	};
}