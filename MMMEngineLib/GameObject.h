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
		friend class App;
		friend class ObjectManager;
		friend class Scene;

		//SceneRef m_scene;

		ObjectPtr<Transform> m_transform;
		std::vector<ObjectPtr<Component>> m_components;

		std::string m_tag = "";
		uint32_t m_layer = static_cast<uint32_t>(-1);

		bool m_active = true;
		bool m_activeInHierarchy = true; // Hierarchy에서 활성화 여부

		void RegisterComponent(ObjectPtr<Component> comp);
		void UnRegisterComponent(ObjectPtr<Component> comp);
		void UpdateActiveInHierarchy();
		void Initialize();
	protected:
		GameObject();
		GameObject(std::string name);
		virtual void BeforeDestroy() override;
	public:
		virtual ~GameObject() = default;
		
		void SetActive(bool active);
		void SetTag(const std::string& tag) { m_tag = tag; }
		void SetLayer(const uint32_t& layer) { m_layer = layer; }

		bool IsActiveSelf() const { return m_active; }
		bool IsActiveInHierarchy() const { return m_activeInHierarchy; }

		const std::string&	GetTag()	const { return m_tag; }
		const uint32_t&		GetLayer()	const { return m_layer; }
		//const SceneRef		GetScene(	const { return m_scene; }

		template <typename T>
		ObjectPtr<T> AddComponent()
		{
			static_assert(!std::is_same_v<T, Transform>, "Transform은 Addcomponent로 생성할 수 없습니다.");
			static_assert(std::is_base_of_v<Component, T>, "T는 Component를 상속받아야 합니다.");

			auto newComponent = Object::CreatePtr<T>();
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
		ObjectPtr<T> GetComponent()
		{
			static_assert(std::is_base_of<Component, T>::value, "GetComponent()의 T는 Component를 상속받아야 합니다.");

			for (auto comp : m_components)
			{
				if (auto typedComp = comp.Cast<T>())
					return typedComp;
			}

			return nullptr;
		}

		const std::vector<ObjectPtr<Component>>& GetAllComponents() const { return m_components; }

		ObjectPtr<Transform> GetTransform() { return m_transform; }

		//static ObjectPtr<GameObject> Find(const std::wstring& name);
		//static ObjectPtr<GameObject> FindWithTag(const std::string& name);
		//static std::vector<ObjectPtr<GameObject>> FindGameObjectsWithTag(const std::string& name);
	};
}