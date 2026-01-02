#pragma once
#include "Object.h"
#include "rttr/type"

namespace MMMEngine
{
	class GameObject : public Object
	{
	private:
		friend class App;
		friend class ObjectManager;
		friend class Scene;
		RTTR_ENABLE(MMMEngine::Object)
		RTTR_REGISTRATION_FRIEND
	protected:
		GameObject() = default;
	public:
		virtual ~GameObject() = default;
	};
}