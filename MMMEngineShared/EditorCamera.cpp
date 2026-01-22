#include "EditorCamera.h"
#include "TimeManager.h"
#include "InputManager.h"
#include "Transform.h"

#include "rttr/registration.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<EditorCamera>("EditorCamera")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<EditorCamera>>()));

	registration::class_<ObjPtr<EditorCamera>>("ObjPtr<EditorCamera>")
		.constructor<>(
			[]() {
				return Object::NewObject<EditorCamera>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<EditorCamera>>();
}

MMMEngine::EditorCamera::EditorCamera()
{
	REGISTER_BEHAVIOUR_MESSAGE(Update);
}

DirectX::SimpleMath::Vector3 MMMEngine::EditorCamera::GetForward()
{
	return -m_CamTrans->GetWorldMatrix().Forward();
}

DirectX::SimpleMath::Vector3 MMMEngine::EditorCamera::GetRight()
{
	return m_CamTrans->GetWorldMatrix().Right();
}

void MMMEngine::EditorCamera::Reset()
{
	m_CamTrans->SetWorldPosition(m_PositionInitial);
	m_CamTrans->SetWorldRotation({0.0f, 0.0f, 0.0f, 0.0f});
	m_CamTrans->SetWorldScale({0.0f, 0.0f, 0.0f});

	m_Rotation = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	m_Position = m_PositionInitial;
}

void MMMEngine::EditorCamera::Update()
{
	DirectX::SimpleMath::Vector3 forward = GetForward();
	DirectX::SimpleMath::Vector3 right = GetRight();

	auto transform = GetGameObject()->GetTransform();

	// 인풋값 받기
	if (InputManager::Get().GetKey(KeyCode::R))
	{
		Reset();
	}

	if (InputManager::Get().GetKey(KeyCode::W))
	{
		AddInputVector(forward);
	}
	else if (InputManager::Get().GetKey(KeyCode::S))
	{
		AddInputVector(-forward);
	}

	if (InputManager::Get().GetKey(KeyCode::A))
	{
		AddInputVector(-right);
	}
	else if (InputManager::Get().GetKey(KeyCode::D))
	{
		AddInputVector(right);
	}

	if (InputManager::Get().GetKey(KeyCode::E))
	{
		AddInputVector(-transform->GetWorldMatrix().Up());
	}
	else if (InputManager::Get().GetKey(KeyCode::Q))
	{
		AddInputVector(-transform->GetWorldMatrix().Up());
	}

	// 매트릭스 만들기
	if (m_InputVector.Length() > 0.0f)
	{
		m_Position += m_InputVector * m_MoveSpeed * TimeManager::Get().GetDeltaTime();
		m_InputVector = DirectX::SimpleMath::Vector3(0.0f, 0.0f, 0.0f);
	}

	m_CamTrans->SetWorldRotation(
		DirectX::SimpleMath::Quaternion::CreateFromYawPitchRoll(m_Rotation));
	m_CamTrans->SetWorldPosition(m_Position);
}

void MMMEngine::EditorCamera::GetViewMatrix(DirectX::SimpleMath::Matrix& out)
{
	auto& worldMat = m_CamTrans->GetWorldMatrix();

	out = worldMat.Invert();
}

void MMMEngine::EditorCamera::AddInputVector(const DirectX::SimpleMath::Vector3& input)
{
	m_InputVector += input;
	m_InputVector.Normalize();
}

void MMMEngine::EditorCamera::AddPitch(float value)
{
	m_Rotation.x += value;
	if (m_Rotation.x > XM_PI)
	{
		m_Rotation.x -= XM_2PI;
	}
	else if (m_Rotation.x < -XM_PI)
	{
		m_Rotation.x += XM_2PI;
	}
}

void MMMEngine::EditorCamera::AddYaw(float value)
{
	m_Rotation.y += value;
	if (m_Rotation.y > XM_PI)
	{
		m_Rotation.y -= XM_2PI;
	}
	else if (m_Rotation.y < -XM_PI)
	{
		m_Rotation.y += XM_2PI;
	}
}

void MMMEngine::EditorCamera::Initialize()
{
	__super::Initialize();

	m_CamTrans = GetGameObject()->GetTransform();
	Reset();
}
