#include "Light.h"
#include "RenderManager.h"

#include "GameObject.h"
#include "rttr/registration.h"
#include "Transform.h"

using namespace DirectX;
using namespace DirectX::SimpleMath;

RTTR_REGISTRATION{
	using namespace rttr;
	using namespace MMMEngine;

	registration::enumeration<LightType>("LightType")
		(
			rttr::value("Directional", LightType::Directional),
			rttr::value("Point", LightType::Point)
			);

	registration::class_<Light>("Light")
		(metadata("wrapper_type_name", "ObjPtr<Light>"))
		.property("Type", &Light::GetLightType, &Light::SetLightType)
		.property("Color", &Light::GetLightColor, &Light::SetLightColor)
		.property("Intensity", &Light::GetLightIntensity, &Light::SetLightIntensity);

	registration::class_ <ObjPtr<Light>>("ObjPtr<Light>")
		.constructor<>(
			[]() {
				return Object::NewObject<Light>();
			});
}

void MMMEngine::Light::SetLightIntensity(const float& _intensity)
{
	float finalIntensity = std::max(0.0f, _intensity);

	m_lightIntensity = finalIntensity;
}

void MMMEngine::Light::SetLightColor(DirectX::SimpleMath::Vector3& _color)
{
	float r = std::max(0.0f, std::min(255.0f, _color.x));
	float g = std::max(0.0f, std::min(255.0f, _color.y));;
	float b = std::max(0.0f, std::min(255.0f, _color.z));;

	m_lightColor = { r, g, b };
}

DirectX::SimpleMath::Vector3 MMMEngine::Light::GetNormalizedColor()
{
	return m_lightColor / 255.0f;
}

void MMMEngine::Light::Initialize()
{
	m_lightIndex = RenderManager::Get().AddLight(this);

	m_properties[L"mLightDir"] = GetTransform()->GetWorldMatrix().Forward();
	m_properties[L"mLightColor"] = GetNormalizedColor();
	m_properties[L"mIntensity"] = m_lightIntensity;

	for (auto& [prop, val] : m_properties) {
		for (int i = 0; i < static_cast<int>(ShaderType::S_END); i++) {
			ShaderInfo::Get().AddAllGlobalPropVal(prop, val);
		}
	}
}

void MMMEngine::Light::UnInitialize()
{
	RenderManager::Get().RemoveLight(m_lightIndex);

	for (auto& [prop, val] : m_properties) {
		for (int i = 0; i < static_cast<int>(ShaderType::S_END); i++) {
			ShaderInfo::Get().RemoveAllGlobalPropVal(prop);
		}
	}
}

void MMMEngine::Light::Render()
{
	m_properties[L"mLightDir"] = GetTransform()->GetWorldMatrix().Forward();
	m_properties[L"mLightColor"] = GetNormalizedColor();
	m_properties[L"mIntensity"] = m_lightIntensity;

	for (auto& [prop, val] : m_properties) {
		for (int i = 0; i < static_cast<int>(ShaderType::S_END); i++) {
			ShaderInfo::Get().AddAllGlobalPropVal(prop, val);
		}
	}
}

bool MMMEngine::Light::IsActiveAndEnabled()
{
	return GetGameObject().IsValid() && GetGameObject()->IsActiveInHierarchy();
}
