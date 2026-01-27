#include "Light.h"
#include "RenderManager.h"

#include "rttr/registration.h"

RTTR_REGISTRATION{
	using namespace rttr;
	using namespace MMMEngine;

	registration::enumeration<LightType>("LightType")
		(
			rttr::value("Directional", LightType::Directional),
			rttr::value("Point", LightType::Point)
			);

	registration::class_<Light>("Light")
		(metadata("wrapper_type", rttr::type::get<ObjPtr<Light>>()))
		.property("Type", &Light::GetLightType, &Light::SetLightType)
		.property("Color", &Light::GetLightColor, &Light::SetLightColor)
		.property("Intensity", &Light::GetLightIntensity, &Light::SetLightIntensity);

	registration::class_ <ObjPtr<Light>>("ObjPtr<Light>")
		.constructor<>(
			[]() {
				return Object::NewObject<Light>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<Light>>();
}

void MMMEngine::Light::SetLightIntensity(const float& _intensity)
{
	float finalIntensity = std::max(0.0f, _intensity);

	lightIntensity = finalIntensity;
}

void MMMEngine::Light::SetLightColor(DirectX::SimpleMath::Vector3& _color)
{
	float r = std::max(0.0f, std::min(255.0f , _color.x));
	float g = std::max(0.0f, std::min(255.0f, _color.y));;
	float b = std::max(0.0f, std::min(255.0f, _color.z));;

	lightColor = {r, g, b};
}

DirectX::SimpleMath::Vector3 MMMEngine::Light::GetNormalizedColor()
{
	return lightColor / 255.0f;
}

void MMMEngine::Light::Initialize()
{
	lightIndex = RenderManager::Get().AddLight(this);
}

void MMMEngine::Light::UnInitialize()
{
	RenderManager::Get().RemoveLight(lightIndex);
}
