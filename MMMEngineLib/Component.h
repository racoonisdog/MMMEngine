#pragma once
#include "Object.h"
#include "rttr/type"

namespace MMMEngine
{
	class Component : public Object
	{
	private:
		RTTR_ENABLE(Object)
		RTTR_REGISTRATION_FRIEND
		friend class ObjectManager;
		friend class GameObject;

		ObjPtr<GameObject> m_gameObject;
		inline void SetGameObject(ObjPtr<GameObject> owner) { m_gameObject = owner; }
	protected:
		Component() = default;
		virtual void Initialize() {};	//생성자 이후 추가 초기화용
		virtual void Dispose() final override;
		virtual void UnInitialize() {};  //파괴 직전 모든 참조 끊기용
	public:
		virtual ~Component() = default;

		inline ObjPtr<GameObject> GetGameObject() { return m_gameObject; };
	};
}