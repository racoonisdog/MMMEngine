#pragma once
#include "Object.h"
#include "rttr/type"
#include <vector>
#include "Export.h"
#include "SceneRef.h"

namespace MMMEngine
{
	class MMMENGINE_API GameObject final : public Object
	{
	private:
		RTTR_ENABLE(Object)
		RTTR_REGISTRATION_FRIEND
		friend class ObjectManager;
		friend class SceneManager;
		friend class Object;
		friend class Scene;
		friend class SceneSerializer;
		friend class Component;
		friend class Transform;

		static uint64_t s_go_instanceID;

		SceneRef m_scene = { static_cast<size_t>(-1), false };

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
		void SetScene(const SceneRef& scene) { m_scene = scene; }
	protected:
		GameObject();
		GameObject(std::string name);
		GameObject(SceneRef scene);
		GameObject(SceneRef scene, std::string name);
		virtual void Construct() override;
		virtual void Dispose() final override;
	public:
		virtual ~GameObject() = default;
		
		void SetActive(bool active);
		void SetTag(const std::string& tag) { m_tag = tag; }
		void SetLayer(const uint32_t& layer) { m_layer = layer; }

		bool IsActiveSelf() const { return m_active; }
		bool IsActiveInHierarchy() const { return m_activeInHierarchy; }
		
		const std::string&	GetTag()		const { return m_tag; }
		const uint32_t&		GetLayer()		const { return m_layer; }
		const SceneRef& GetScene() const { return m_scene; }

		ObjPtr<Component> AddComponent(rttr::type compType);

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

			for (auto& comp : m_components)
			{
				if (!comp.IsValid() || comp->IsDestroyed())
					continue;

				if (auto typedComp = comp.Cast<T>())
					return typedComp;
			}

			return nullptr;
		}

		template <typename T>
		std::vector<ObjPtr<T>> GetComponents()
		{
			static_assert(std::is_base_of<Component, T>::value, "GetComponent()의 T는 Component를 상속받아야 합니다.");

			std::vector<ObjPtr<T>> result;

			for (auto& comp : m_components)
			{
				if (!comp.IsValid() || comp->IsDestroyed())
					continue;

				if (auto typedComp = comp.Cast<T>())
					result.push_back(typedComp);
			}

			return result;
		}

		template <typename T>
		size_t GetComponentsCount()
		{
			static_assert(std::is_base_of<Component, T>::value, "GetComponent()의 T는 Component를 상속받아야 합니다.");

			size_t result = 0;

			for (auto& comp : m_components)
			{
				if (!comp.IsValid() || comp->IsDestroyed())
					continue;

				if (auto typedComp = comp.Cast<T>())
					++result;
			}

			return result;
		}


		const std::vector<ObjPtr<Component>>& GetAllComponents() const { return m_components; }

		ObjPtr<Transform> GetTransform() { return m_transform; }

		static ObjPtr<GameObject> Find(const std::string& name);
		static ObjPtr<GameObject> FindWithTag(const std::string& tag);
		static std::vector<ObjPtr<GameObject>> FindGameObjectsWithTag(const std::string& tag);
	};
}