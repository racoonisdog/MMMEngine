#include "Camera.h"
#include "Transform.h"
#include "rttr/registration"
#include "SceneManager.h"

MMMEngine::ObjPtr<MMMEngine::Camera> MMMEngine::Camera::s_mainCam = nullptr;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Camera>("Camera")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<Camera>>()))
		.property("FOV", &Camera::GetFov, &Camera::SetFOV)
		.property("Near", &Camera::GetNear, &Camera::SetNear)
		.property("Far", &Camera::GetFar, &Camera::SetFar)
		.property("AspectRatio", &Camera::GetAsepct, &Camera::SetAspect);


	registration::class_<ObjPtr<Camera>>("ObjPtr<Camera>")
		.constructor<>(
			[]() {
				return Object::NewObject<Camera>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<Camera>>();
}

void MMMEngine::Camera::MarkViewMatrixDirty()
{
	m_isViewMatrixDirty = true;
}

void MMMEngine::Camera::MarkProjectionMatrixDirty()
{
	m_isProjMatrixDirty = true;
}

const DirectX::SimpleMath::Matrix MMMEngine::Camera::GetCameraMatrix()
{
	return GetViewMatrix() * GetProjMatrix();
}

const DirectX::SimpleMath::Matrix& MMMEngine::Camera::GetViewMatrix()
{
	if (m_isViewMatrixDirty)
	{
		if (GetTransform().IsValid())
		{
			m_cachedViewMatrix = GetTransform()->GetWorldMatrix().Invert();
			m_isViewMatrixDirty = false;
		}
	}

	return m_cachedViewMatrix;
}

const DirectX::SimpleMath::Matrix& MMMEngine::Camera::GetProjMatrix()
{
	if (m_isProjMatrixDirty)
	{
		UpdateProjMatrix();
		m_isProjMatrixDirty = false;
	}

	return m_cachedProjMatrix;
}

void MMMEngine::Camera::UpdateProjMatrix()
{
	XMStoreFloat4x4(&m_cachedProjMatrix, DirectX::XMMatrixPerspectiveFovLH(DirectX::XMConvertToRadians(m_fov), m_aspect, m_near, m_far));
}

void MMMEngine::Camera::Initialize()
{
	Behaviour::Initialize();

	if (GetTransform().IsValid())
	{
		GetTransform()->onMatrixUpdate.AddListener<Camera, &Camera::MarkViewMatrixDirty>(this);
		GetTransform()->onMatrixUpdate.AddListener<Camera, &Camera::MarkProjectionMatrixDirty>(this);
	}
		

	m_fov = 75;
	m_near = 0.3f;
	m_far = 1000.0f;
	m_aspect = 4 / 3;

	MarkViewMatrixDirty();
}


void MMMEngine::Camera::UnInitialize()
{
	Behaviour::UnInitialize();

	if (GetTransform().IsValid())
	{
		GetTransform()->onMatrixUpdate.RemoveListener<Camera, &Camera::MarkViewMatrixDirty>(this);
		GetTransform()->onMatrixUpdate.RemoveListener<Camera, &Camera::MarkProjectionMatrixDirty>(this);
	}
}

const float& MMMEngine::Camera::GetFov() noexcept
{
	return m_fov;
}

const float& MMMEngine::Camera::GetNear() noexcept
{
	return m_near;
}

const float& MMMEngine::Camera::GetFar() noexcept
{
	return m_far;
}

const float& MMMEngine::Camera::GetAsepct() noexcept
{
	return m_aspect;
}

void MMMEngine::Camera::SetFOV(const float& value)
{
	MarkProjectionMatrixDirty();
	m_fov = value;
}

void MMMEngine::Camera::SetNear(const float& value)
{
	MarkProjectionMatrixDirty();
	m_near = value;
}

void MMMEngine::Camera::SetFar(const float& value)
{
	MarkProjectionMatrixDirty();
	m_far = value;
}

void MMMEngine::Camera::SetAspect(const float& value)
{
	MarkProjectionMatrixDirty();
	m_aspect = value;
}

MMMEngine::ObjPtr<MMMEngine::Camera> MMMEngine::Camera::GetMainCamera()
{
	if (!s_mainCam.IsValid() || s_mainCam->IsDestroyed())
	{
		//새로운 메인캠 찾기
		auto mainCamGOs = GameObject::FindGameObjectsWithTag("MainCamera");

		if (mainCamGOs.size() != 0)
			for (auto& camGO : mainCamGOs)
			{
				if (auto mainCam = camGO->GetComponent<Camera>())
				{
					s_mainCam = mainCam;
					break;
				}
			}
	}
	else
	{
		s_mainCam = nullptr;
	}

	return s_mainCam;
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::Camera::CreateMainCamera()
{
	//카메라 오브젝트를 생성할 씬이 없음
	if (SceneManager::Get().GetCurrentScene().id == static_cast<size_t>(-1))
		return nullptr;

	auto cameraGO = NewObject<GameObject>("MainCamera");
	cameraGO->SetTag("MainCamera");
	auto cam = cameraGO->AddComponent<Camera>();
	//auto listener = cameraGO->AddComponent<AudioListener>();
	//listener->SetAsMainListener();

	if (!s_mainCam)
		s_mainCam = cam;

	return cameraGO;
}
