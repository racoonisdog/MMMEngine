#pragma once
#include "Object.h"
#include "rttr/type"
#include <vector>
#include "Component.h"

namespace MMMEngine
{
	//class Transform;
	class GameObject : public Object
	{
	private:
		//ObjectPtr<Transform> m_transform;
		std::vector<ObjectPtr<Component>> m_components;
		friend class App;
		friend class ObjectManager;
		friend class Scene;
		RTTR_ENABLE(MMMEngine::Object)
		RTTR_REGISTRATION_FRIEND
	protected:
		GameObject() = default;
		GameObject(std::string name);
	public:
		virtual ~GameObject() = default;
	};
}