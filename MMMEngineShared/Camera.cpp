#include "Camera.h"
#include "AudioListener.h"
#include "Transform.h"
#include "rttr/registration"
#include "SceneManager.h"
#include "RenderManager.h"

MMMEngine::ObjPtr<MMMEngine::Camera> MMMEngine::Camera::s_mainCam = nullptr;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Camera>("Camera")
		(rttr::metadata("wrapper_type_name", "ObjPtr<Camera>"))
		.property("FOV", &Camera::GetFov, &Camera::SetFOV)
		.property("Near", &Camera::GetNear, &Camera::SetNear)
		.property("Far", &Camera::GetFar, &Camera::SetFar);
		// todo :  AsepectRatio <- 카메라가 직접 설정하면 안됨, RenderManager의 씬타겟 이미지의 해상도로 처리해주셈
		//.property("AspectRatio", &Camera::GetAsepct, &Camera::SetAspect);


	registration::class_<ObjPtr<Camera>>("ObjPtr<Camera>")
		.constructor<>(
			[]() {
				return Object::NewObject<Camera>();
			})
		.method("Inject", &ObjPtr<Camera>::Inject);
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

DirectX::SimpleMath::Vector3 MMMEngine::Camera::WorldToScreenPoint(
	const DirectX::SimpleMath::Vector3& worldPos)
{
	using namespace DirectX::SimpleMath;
	UINT w = 0, h = 0;
	RenderManager::Get().GetSceneSize(w, h);
	const float screenWidth = static_cast<float>(w);
	const float screenHeight = static_cast<float>(h);
	if (screenWidth <= 0.0f || screenHeight <= 0.0f)
		return Vector3::Zero;

	const Matrix view = GetViewMatrix();
	const Matrix proj = GetProjMatrix();

	const Vector4 viewPos = Vector4::Transform(Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f), view);
	const Matrix viewProj = view * proj;
	const Vector4 clip = Vector4::Transform(Vector4(worldPos.x, worldPos.y, worldPos.z, 1.0f), viewProj);
	if (std::abs(clip.w) < 1e-6f)
		return Vector3(0.0f, 0.0f, viewPos.z);

	const float invW = 1.0f / clip.w;
	const float ndcX = clip.x * invW;
	const float ndcY = clip.y * invW;

	const float screenX = (ndcX * 0.5f + 0.5f) * screenWidth;
	const float screenY = (1.0f - (ndcY * 0.5f + 0.5f)) * screenHeight;
	return { screenX, screenY, viewPos.z };
}

DirectX::SimpleMath::Vector3 MMMEngine::Camera::ScreenToWorldPoint(
	const DirectX::SimpleMath::Vector2& screenPos,
	float screenWidth,
	float screenHeight,
	float viewDepth)
{
	using namespace DirectX::SimpleMath;
	if (screenWidth <= 0.0f || screenHeight <= 0.0f)
		return Vector3::Zero;

	const float ndcX = (screenPos.x / screenWidth) * 2.0f - 1.0f;
	const float ndcY = 1.0f - (screenPos.y / screenHeight) * 2.0f;

	const Matrix proj = GetProjMatrix();
	float ndcZ = 0.0f;
	if (std::abs(proj._44) < 1e-6f)
	{
		// Perspective
		if (std::abs(viewDepth) < 1e-6f)
			return Vector3::Zero;
		ndcZ = proj._33 + proj._43 / viewDepth;
	}
	else
	{
		// Orthographic
		ndcZ = viewDepth * proj._33 + proj._43;
	}

	const Matrix viewProj = GetViewMatrix() * proj;
	const Matrix invViewProj = viewProj.Invert();
	const Vector4 world = Vector4::Transform(Vector4(ndcX, ndcY, ndcZ, 1.0f), invViewProj);
	if (std::abs(world.w) < 1e-6f)
		return Vector3::Zero;
	const float invW = 1.0f / world.w;
	return Vector3(world.x * invW, world.y * invW, world.z * invW);
}

DirectX::SimpleMath::Vector3 MMMEngine::Camera::ScreenToWorldPoint(
	const DirectX::SimpleMath::Vector2& screenPos,
	float viewDepth)
{
	UINT w = 0, h = 0;
	RenderManager::Get().GetSceneSize(w, h);
	return ScreenToWorldPoint(screenPos, static_cast<float>(w), static_cast<float>(h), viewDepth);
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

	m_fov = 75.0f;
	m_near = 0.3f;
	m_far = 1000.0f;

	// 현재 씬 렌더 타깃 크기에서 Aspect를 가져온다.
	UINT w = 0, h = 0;
	RenderManager::Get().GetSceneSize(w, h);
	if (w > 0 && h > 0)
		m_aspect = static_cast<float>(w) / static_cast<float>(h);
	else
		m_aspect = 16.0f / 9.0f;   // fallback

	MarkViewMatrixDirty();
	MarkProjectionMatrixDirty();  // 초기 Projection도 다시 계산하도록 플래그

	RenderManager::Get().SetCamera(SelfPtr(this));
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
		auto mainCamGOs = GameObject::FindGameObjectsWithTag("MainCamera");

		for (auto& camGO : mainCamGOs)
		{
			if (auto mainCam = camGO->GetComponent<Camera>())
			{
				s_mainCam = mainCam;
				break;
			}
		}
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
	auto listener = cameraGO->AddComponent<AudioListener>();
	listener->SetAsMainListener();

	if (!s_mainCam)
		s_mainCam = cam;

	return cameraGO;
}
