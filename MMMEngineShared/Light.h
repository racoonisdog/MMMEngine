#pragma once
#include "Export.h"
#include "Component.h"
#include "rttr/type"
#include <SimpleMath.h>

namespace MMMEngine {
	// 라이트 타입
	enum LightType {
		Directional = 0,
		Point = 1,
	};

	class RenderManager;
	class MMMENGINE_API Light : public Component
	{
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
		friend class RenderManager;
	private:
		LightType lightType;
		int lightIndex = -1;
		DirectX::SimpleMath::Vector3 lightColor;
		float lightIntensity = 1.0f;

	public:
		const float& GetLightIntensity() { return lightIntensity; }
		void SetLightIntensity(const float& _intensity);

		LightType GetLightType() { return lightType; }
		void SetLightType(LightType _type) { lightType = _type; }

		DirectX::SimpleMath::Vector3& GetLightColor() { return lightColor; }
		void SetLightColor(DirectX::SimpleMath::Vector3& _color);
		DirectX::SimpleMath::Vector3 GetNormalizedColor();

		void Initialize() override;
		void UnInitialize() override;
	};
}


