#pragma once
#include "Component.h"
#include "rttr/type"

namespace MMMEngine {
	class RenderManager;
	class MMMENGINE_API Renderer : public Component
	{
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
		friend class RenderManager;
	protected:
		uint32_t renderIndex = -1;
		bool isEnabled = true;

		virtual void Render() {}
		virtual void Init() {}
		virtual ~Renderer() {}

	public:
		bool GetEnabled() { return isEnabled; }
		void SetEnabled(bool _val) { isEnabled = _val; }

		bool IsActiveAndEnabled();
	};
}


