#pragma once
#include "Export.h"
#include "Component.h"
#include "rttr/type"
#include "SimpleMath.h"
#include "Delegates.hpp"
#include "PhysxManager.h"

namespace MMMEngine
{
	class MMMENGINE_API Transform : public Component
	{
	private:
		RTTR_ENABLE(Component)
		RTTR_REGISTRATION_FRIEND
		friend class ObjectManager;
		friend class GameObject;
		
		DirectX::SimpleMath::Vector3 m_localPosition;
		DirectX::SimpleMath::Quaternion m_localRotation;
		DirectX::SimpleMath::Vector3 m_localScale;

		mutable bool m_isLocalMatDirty;
		mutable bool m_isWorldMatDirty;
		mutable DirectX::SimpleMath::Matrix m_cachedLocalMat;
		mutable DirectX::SimpleMath::Matrix m_cachedWorldMat;

		ObjPtr<Transform> m_parent;
		std::vector<ObjPtr<Transform>> m_childs;

		void AddChild(ObjPtr<Transform> child);
		void RemoveChild(ObjPtr<Transform> child);
		void MarkDirty();
	protected:
		Transform();
		//virtual void Initialize() override {};
		//virtual void UnInitialize() override;
	public:
		virtual ~Transform() = default;

		Utility::Event<Transform, void(void)> onMatrixUpdate{ this };
		Utility::Event<Transform, void(ObjPtr<Transform>)> onUpdateTransformTree{ this };

		const DirectX::SimpleMath::Matrix& GetLocalMatrix() const;
		const DirectX::SimpleMath::Matrix& GetWorldMatrix() const;

		const DirectX::SimpleMath::Vector3& GetLocalPosition() const;
		const DirectX::SimpleMath::Quaternion& GetLocalRotation() const;
		const DirectX::SimpleMath::Vector3 GetLocalEulerRotation() const; // return degree (0 ~ 360)
		const DirectX::SimpleMath::Vector3& GetLocalScale() const;

		const DirectX::SimpleMath::Vector3 GetWorldPosition() const;
		const DirectX::SimpleMath::Quaternion GetWorldRotation() const;
		const DirectX::SimpleMath::Vector3 GetWorldEulerRotation() const; // return degree (0 ~ 360)
		const DirectX::SimpleMath::Vector3 GetWorldScale() const;
			 
		void SetWorldPosition(const DirectX::SimpleMath::Vector3& pos);
		inline void SetWorldPosition(float x, float y, float z) { SetWorldPosition({ x,y,z }); }

		void SetWorldRotation(const DirectX::SimpleMath::Quaternion& rot);
		inline void SetWorldRotation(float x, float y, float z, float w) { SetWorldRotation({ x,y,z,w }); }

		void SetWorldEulerRotation(const DirectX::SimpleMath::Vector3& rot); // use degree (0 ~ 360)
		inline void SetWorldEulerRotation(float x, float y, float z) { SetWorldEulerRotation({ x,y,z }); } // use degree (0 ~ 360)

		void SetWorldScale(const DirectX::SimpleMath::Vector3& scale);
		inline void SetWorldScale(float x, float y, float z) { SetWorldScale({ x,y,z }); }

		void SetLocalPosition(const DirectX::SimpleMath::Vector3& pos);
		inline void SetLocalPosition(float x, float y, float z) { SetLocalPosition({ x,y,z }); }

		void SetLocalRotation(const DirectX::SimpleMath::Quaternion& rot);
		inline void SetLocalRotation(float x, float y, float z, float w) { SetLocalRotation({ x,y,z,w }); }

		void SetLocalScale(const DirectX::SimpleMath::Vector3& scale);
		inline void SetLocalScale(float x, float y, float z) { SetLocalScale({ x,y,z }); }

		ObjPtr<Transform> GetParent() const;
		ObjPtr<Transform> GetChild(size_t index) const;
		size_t			  GetChildCount() const;

		void SetParent(ObjPtr<Transform> parent, bool worldPositionStays = true);

		ObjPtr<Transform> Find(const std::string& path);
		ObjPtr<Transform> GetRoot();
		void DetachChildren();

		virtual void SetSiblingIndex(size_t idx);
		size_t GetSiblingIndex() const;
	};
}
