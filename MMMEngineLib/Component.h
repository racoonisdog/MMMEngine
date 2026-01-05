#pragma once
#include "Object.h"
#include "rttr/type"

namespace MMMEngine
{
	class Component : public Object
	{
	private:
		friend class App;
		friend class ObjectManager;
		friend class GameObject;
		RTTR_ENABLE(MMMEngine::Object)
		RTTR_REGISTRATION_FRIEND
	protected:
		Component() = default;
	public:
		virtual ~Component() = default;
	};
}