#pragma once
#include "Object.h"
#include "rttr/type"
#include <vector>

namespace MMMEngine
{
	class Transform;
	class Component;
	class GameObject : public Object
	{
	private:
		RTTR_ENABLE(Object)
		RTTR_REGISTRATION_FRIEND
		friend class ObjectManager;
		friend class Scene;
		friend class Component;

		//SceneRef m_scene;

		ObjPtr<Transform> m_transform;
		std::vector<ObjPtr<Component>> m_components;

		std::string m_tag = "";
		uint32_t m_layer = static_cast<uint32_t>(-1);

		bool m_active = true;
		bool m_activeInHierarchy = true; // Hierarchy에서 활성화 여부

		void RegisterComponent(const ObjPtr<Component>& comp);
		void UnRegisterComponent(const ObjPtr<Component>& comp);
		void UpdateActiveInHierarchy();
		void Initialize();
		std::vector<ObjPtr<Component>> GetComponentsCopy() { return m_components; }
	protected:
		GameObject();
		GameObject(std::string name);
		virtual void Dispose() final override;
	public:
		virtual ~GameObject() = default;
		
		void SetActive(bool active);
		void SetTag(const std::string& tag) { m_tag = tag; }
		void SetLayer(const uint32_t& layer) { m_layer = layer; }

		bool IsActiveSelf() const { return m_active; }
		bool IsActiveInHierarchy() const { return m_activeInHierarchy; }

		const std::string&	GetTag()	const { return m_tag; }
		const uint32_t&		GetLayer()	const { return m_layer; }
		//const SceneRef		GetScene()	const { return m_scene; }

		template <typename T>
		ObjPtr<T> AddComponent()
		{
			static_assert(!std::is_same_v<T, Transform>, "Transform은 Addcomponent로 생성할 수 없습니다.");
			static_assert(std::is_base_of_v<Component, T>, "T는 Component를 상속받아야 합니다.");

			auto newComponent = Object::NewObject<T>();
			newComponent->m_gameObject = SelfPtr(this);
			RegisterComponent(newComponent);

			//if constexpr (std::is_same<T, Canvas>::value || std::is_base_of<Graphic, T>::value)
			//{
			//	EnsureRectTransform();
			//	UpdateActiveInHierarchy();
			//}

			newComponent.Cast<Component>()->Initialize();

			return newComponent;
		}

		template <typename T>
		ObjPtr<T> GetComponent()
		{
			static_assert(std::is_base_of<Component, T>::value, "GetComponent()의 T는 Component를 상속받아야 합니다.");

			for (auto comp : m_components)
			{
				if (!comp.IsValid() || comp->IsDestroyed())
					continue;

				if (auto typedComp = comp.Cast<T>())
					return typedComp;
			}

			return nullptr;
		}

		const std::vector<ObjPtr<Component>>& GetAllComponents() const { return m_components; }

		ObjPtr<Transform> GetTransform() { return m_transform; }

		//static ObjPtr<GameObject> Find(const std::wstring& name);
		//static ObjPtr<GameObject> FindWithTag(const std::string& name);
		//static std::vector<ObjPtr<GameObject>> FindGameObjectsWithTag(const std::string& name);
	};
}