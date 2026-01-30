#include "GameObject.h"
#include "Object.h"
#include "Component.h"
#include "MissingScriptBehaviour.h"
#include "rttr/registration"
#include "rttr/type"
#include "rttr/detail/policies/ctor_policies.h"
#include "ObjectManager.h"
#include <stdexcept>
#include <iostream>
#include <unordered_map>
#include <vector>
#include "Transform.h"
#include "SceneManager.h"

using namespace MMMEngine::Utility;

uint64_t MMMEngine::Object::s_nextInstanceID = 1;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Object>("Object")
		.property("Name", &Object::GetName, &Object::SetName)(rttr::metadata("INSPECTOR", "HIDDEN"))
		.property("MUID", &Object::GetMUID, &Object::SetMUID)
		.property_readonly("InstanceID", &Object::GetInstanceID)
		.property_readonly("isDestroyed", &Object::IsDestroyed)(rttr::metadata("INSPECTOR", "HIDDEN"));

	registration::class_<ObjPtrBase>("ObjPtr")
		.method("IsValid", &ObjPtrBase::IsValid)
		.method("GetRaw", &ObjPtrBase::GetRaw, registration::private_access)
		.method("GetPtrID", &ObjPtrBase::GetPtrID)
		.method("GetPtrGeneration", &ObjPtrBase::GetPtrGeneration);

	registration::class_<ObjPtr<Object>>("ObjPtr<Object>")
		.constructor<>(
			[]() { 
				return Object::NewObject<Object>(); 
			})
		.method("Inject", &ObjPtr<Object>::Inject);
}

namespace
{
	using namespace MMMEngine;
	using namespace rttr;

	struct CloneContext
	{
		std::unordered_map<const Object*, ObjPtr<Object>> objectMap;
		std::vector<std::pair<ObjPtr<Component>, ObjPtr<Component>>> componentPairs;
		std::vector<std::pair<ObjPtr<Transform>, ObjPtr<Transform>>> transformPairs;
	};

	void CollectHierarchy(const ObjPtr<GameObject>& root, std::vector<ObjPtr<GameObject>>& out)
	{
		if (!root.IsValid() || root->IsDestroyed())
			return;

		std::vector<ObjPtr<GameObject>> stack;
		stack.push_back(root);

		while (!stack.empty())
		{
			ObjPtr<GameObject> current = stack.back();
			stack.pop_back();

			if (!current.IsValid() || current->IsDestroyed())
				continue;

			out.push_back(current);

			auto tr = current->GetTransform();
			if (!tr.IsValid())
				continue;

			const size_t childCount = tr->GetChildCount();
			for (size_t i = childCount; i-- > 0; )
			{
				auto childTr = tr->GetChild(i);
				if (!childTr.IsValid())
					continue;

				auto childGo = childTr->GetGameObject();
				if (childGo.IsValid() && !childGo->IsDestroyed())
					stack.push_back(childGo);
			}
		}
	}

	ObjPtr<Transform> FindMappedTransform(const CloneContext& ctx, const ObjPtr<Transform>& original)
	{
		if (!original.IsValid())
			return ObjPtr<Transform>();

		auto it = ctx.objectMap.find(original.operator->());
		if (it == ctx.objectMap.end())
			return ObjPtr<Transform>();

		return it->second.Cast<Transform>();
	}

	rttr::variant CloneVariant(const rttr::variant& src, const rttr::type& targetType, const CloneContext& ctx);

	void CloneObject(rttr::instance srcObj, rttr::instance dstObj, const CloneContext& ctx)
	{
		type t = srcObj.get_derived_type();
		const bool isObjectDerived =
			t.is_derived_from(type::get<Object>()) ||
			t == type::get<Object>();

		for (auto& prop : t.get_properties(
			rttr::filter_item::instance_item |
			rttr::filter_item::public_access |
			rttr::filter_item::non_public_access))
		{
			if (prop.is_readonly())
				continue;

			const std::string propName = prop.get_name().to_string();

			if (isObjectDerived && propName == "MUID")
				continue;

			if (t == type::get<Transform>() &&
				(propName == "Parent" || propName == "m_parent"))
				continue;

			rttr::variant value = prop.get_value(srcObj);
			rttr::variant cloned = CloneVariant(value, prop.get_type(), ctx);
			prop.set_value(dstObj, cloned);
		}
	}

	rttr::variant CloneVariant(const rttr::variant& src, const rttr::type& targetType, const CloneContext& ctx)
	{
		if (!src.is_valid())
			return rttr::variant();

		if (targetType.is_enumeration())
			return src;

		if (targetType.is_arithmetic())
		{
			if (src.get_type() == targetType)
				return src;

			rttr::variant converted = src;
			if (converted.convert(targetType))
				return converted;

			return src;
		}

		if (targetType == type::get<std::string>() ||
			targetType == type::get<MMMEngine::Utility::MUID>())
		{
			return src;
		}

		if (targetType.get_name().to_string().find("ObjPtr") != std::string::npos)
		{
			auto inject = targetType.get_method("Inject");
			rttr::variant target = targetType.create();
			if (!inject.is_valid() || !target.is_valid())
				return rttr::variant();

			Object* raw = nullptr;
			if (!src.convert(raw) || raw == nullptr || raw->IsDestroyed())
			{
				ObjPtr<Object> nullObj;
				const ObjPtrBase& nullRef = nullObj;
				inject.invoke(target, nullRef);
				return target;
			}

			auto it = ctx.objectMap.find(raw);
			ObjPtr<Object> mapped = (it != ctx.objectMap.end())
				? it->second
				: ObjectManager::Get().GetPtrFromRaw<Object>(raw);

			if (!mapped.IsValid())
			{
				ObjPtr<Object> nullObj;
				const ObjPtrBase& nullRef = nullObj;
				inject.invoke(target, nullRef);
				return target;
			}

			const ObjPtrBase& baseRef = mapped;
			inject.invoke(target, baseRef);
			return target;
		}

		if (targetType.is_sequential_container())
		{
			rttr::variant target = targetType.create();
			if (!target.is_valid())
				return src;

			auto view = target.create_sequential_view();
			view.clear();

			auto args = targetType.get_wrapped_type().get_template_arguments();
			auto it = args.begin();
			if (it == args.end())
				return target;

			rttr::type valueType = *it;

			auto srcView = src.create_sequential_view();
			for (const auto& item : srcView)
			{
				rttr::variant cloned = CloneVariant(item, valueType, ctx);
				view.insert(view.end(), cloned);
			}

			return target;
		}

		if (targetType.is_associative_container())
		{
			rttr::variant target = targetType.create();
			if (!target.is_valid())
				return src;

			auto view = target.create_associative_view();
			view.clear();

			auto args = targetType.get_wrapped_type().get_template_arguments();
			auto it = args.begin();
			if (it == args.end())
				return target;

			rttr::type keyType = *it;
			++it;
			if (it == args.end())
				return target;

			rttr::type valueType = *it;

			auto srcView = src.create_associative_view();
			for (auto& item : srcView)
			{
				rttr::variant key = CloneVariant(item.first, keyType, ctx);
				rttr::variant value = CloneVariant(item.second, valueType, ctx);
				view.insert(key, value);
			}

			return target;
		}

		if (targetType.is_wrapper())
			return src;

		auto props = targetType.get_properties(
			rttr::filter_item::instance_item |
			rttr::filter_item::public_access |
			rttr::filter_item::non_public_access);

		if (props.begin() == props.end())
			return src;

		rttr::variant target = targetType.create();
		if (!target.is_valid())
			return src;

		CloneObject(src, target, ctx);
		return target;
	}

	ObjPtr<GameObject> CreateCloneShallow(const ObjPtr<GameObject>& original, CloneContext& ctx)
	{
		if (!original.IsValid() || original->IsDestroyed())
			return ObjPtr<GameObject>();

		SceneRef sceneRef = original->GetScene();
		ObjPtr<GameObject> clone = Object::NewObject<GameObject>(sceneRef, original->GetName());

		if (auto sceneRaw = SceneManager::Get().GetSceneRaw(sceneRef))
			sceneRaw->RegisterGameObject(clone);

		clone->SetName(original->GetName());
		clone->SetTag(original->GetTag());
		clone->SetLayer(original->GetLayer());
		clone->SetActive(original->IsActiveSelf());

		ctx.objectMap.emplace(original.operator->(), ObjPtr<Object>(clone));

		auto origTr = original->GetTransform();
		auto cloneTr = clone->GetTransform();
		if (origTr.IsValid() && cloneTr.IsValid())
		{
			ctx.objectMap.emplace(origTr.operator->(), ObjPtr<Object>(cloneTr));
			ctx.transformPairs.emplace_back(origTr, cloneTr);
		}

		// RigidBody를 먼저 만들고, 그 다음 나머지 컴포넌트를 생성한다.
		// Collider가 먼저 만들어지면 자동으로 RigidBody가 생성되어 복제값이 덮이는 문제 방지.
		for (auto& comp : original->GetAllComponents())
		{
			if (!comp.IsValid() || comp->IsDestroyed())
				continue;

			if (comp.Cast<Transform>())
				continue;

			rttr::type compType = rttr::type::get(*comp);
			if (compType.get_name().to_string() != "RigidBodyComponent")
				continue;

			ObjPtr<Component> clonedComp = clone->AddComponent(compType);
			if (!clonedComp.IsValid())
				continue;

			ctx.objectMap.emplace(comp.operator->(), ObjPtr<Object>(clonedComp));
			ctx.componentPairs.emplace_back(comp, clonedComp);
		}

		for (auto& comp : original->GetAllComponents())
		{
			if (!comp.IsValid() || comp->IsDestroyed())
				continue;

			if (comp.Cast<Transform>())
				continue;

			rttr::type compType = rttr::type::get(*comp);
			if (compType.get_name().to_string() == "RigidBodyComponent")
				continue;

			ObjPtr<Component> clonedComp = clone->AddComponent(compType);
			if (!clonedComp.IsValid())
				continue;

			ctx.objectMap.emplace(comp.operator->(), ObjPtr<Object>(clonedComp));
			ctx.componentPairs.emplace_back(comp, clonedComp);
		}

		return clone;
	}

	ObjPtr<GameObject> InstantiateGameObjectInternal(const ObjPtr<GameObject>& original, CloneContext& ctx)
	{
		std::vector<ObjPtr<GameObject>> originals;
		CollectHierarchy(original, originals);
		if (originals.empty())
			return ObjPtr<GameObject>();

		ObjPtr<GameObject> rootClone;
		for (auto& go : originals)
		{
			ObjPtr<GameObject> clone = CreateCloneShallow(go, ctx);
			if (!clone.IsValid())
				continue;

			if (go == original)
				rootClone = clone;
		}

		for (auto& pair : ctx.transformPairs)
		{
			auto& srcTr = pair.first;
			auto& dstTr = pair.second;
			if (!srcTr.IsValid() || !dstTr.IsValid())
				continue;

			dstTr->SetLocalPosition(srcTr->GetLocalPosition());
			dstTr->SetLocalRotation(srcTr->GetLocalRotation());
			dstTr->SetLocalScale(srcTr->GetLocalScale());
		}

		for (auto& pair : ctx.componentPairs)
		{
			auto& srcComp = pair.first;
			auto& dstComp = pair.second;
			if (!srcComp.IsValid() || !dstComp.IsValid())
				continue;

			CloneObject(*srcComp, *dstComp, ctx);

			auto missingSrc = srcComp.Cast<MissingScriptBehaviour>();
			if (missingSrc.IsValid())
			{
				auto missingDst = dstComp.Cast<MissingScriptBehaviour>();
				if (missingDst.IsValid())
					missingDst->SetOriginalPropsMsgPack(missingSrc->GetOriginalPropsMsgPack());
			}
		}

		for (auto& go : originals)
		{
			if (!go.IsValid() || go->IsDestroyed())
				continue;

			auto origTr = go->GetTransform();
			auto cloneTr = FindMappedTransform(ctx, origTr);
			if (!cloneTr.IsValid())
				continue;

			const size_t childCount = origTr->GetChildCount();
			for (size_t i = 0; i < childCount; ++i)
			{
				auto childTr = origTr->GetChild(i);
				auto childCloneTr = FindMappedTransform(ctx, childTr);
				if (!childCloneTr.IsValid())
					continue;

				childCloneTr->SetParent(cloneTr, false);
			}
		}

		if (rootClone.IsValid())
		{
			auto origRootParent = original->GetTransform()->GetParent();
			if (origRootParent.IsValid() && !origRootParent->IsDestroyed())
			{
				if (!FindMappedTransform(ctx, origRootParent).IsValid())
					rootClone->GetTransform()->SetParent(origRootParent, false);
			}
		}

		return rootClone;
	}
}

MMMEngine::Object::Object() : m_instanceID(s_nextInstanceID++)
{
	if (!ObjectManager::Get().IsCreatingObject())
	{
		throw std::runtime_error(
			"Object는 CreatePtr로만 생성할 수 있습니다.\n"
			"스택 생성이나 직접 new 사용이 감지되었습니다."
		);
	}

	m_name = "<Unnamed> [ Instance ID : " + std::to_string(m_instanceID) + " ]";
	m_muid = MUID::NewMUID();
	m_ptrID = UINT32_MAX;
	m_ptrGen = 0;
}

MMMEngine::Object::~Object()
{
	if (!ObjectManager::Get().IsDestroyingObject())
	{
#ifdef _DEBUG
		// Debug: 디버거 중단 + 스택 트레이스
		std::cerr << "\n=== 오브젝트 파괴 오류 ===" << std::endl;
		std::cerr << "Object는 Destroy로만 파괴할 수 있습니다." << std::endl;
		std::cerr << "직접 delete 사용이 감지되었습니다." << std::endl;
		std::cerr << "\n>>> 호출 스택 확인 <<<\n" << std::endl;
		__debugbreak();
#endif
		// Release와 Debug 모두: 즉시 종료
		std::cerr << "\nFATAL ERROR: 허용되지 않는 방법으로 오브젝트 파괴" << std::endl;
		std::abort(); 
	}
}

MMMEngine::ObjPtr<MMMEngine::GameObject> MMMEngine::Object::Instantiate(const ObjPtr<GameObject>& original)
{
	if (!original.IsValid() || original->IsDestroyed())
		return ObjPtr<GameObject>();

	CloneContext ctx;
	return InstantiateGameObjectInternal(original, ctx);
}

MMMEngine::ObjPtr<MMMEngine::Component> MMMEngine::Object::Instantiate(const ObjPtr<Component>& original)
{
	if (!original.IsValid() || original->IsDestroyed())
		return ObjPtr<Component>();

	auto owner = original->GetGameObject();
	if (!owner.IsValid() || owner->IsDestroyed())
		return ObjPtr<Component>();

	CloneContext ctx;
	ObjPtr<GameObject> clonedRoot = InstantiateGameObjectInternal(owner, ctx);
	if (!clonedRoot.IsValid())
		return ObjPtr<Component>();

	auto it = ctx.objectMap.find(original.operator->());
	if (it != ctx.objectMap.end())
		return it->second.Cast<Component>();

	rttr::type originalType = rttr::type::get(*original);
	for (auto& comp : clonedRoot->GetAllComponents())
	{
		if (!comp.IsValid() || comp->IsDestroyed())
			continue;

		if (rttr::type::get(*comp) == originalType)
			return comp;
	}

	return ObjPtr<Component>();
}

void MMMEngine::Object::DontDestroyOnLoad(const ObjPtrBase& objPtr)
{
	// GameObject인 경우 그 자체를 씬에게 넘기기
	if (auto go = ObjectManager::Get().GetPtr<Object>(objPtr.GetPtrID(), objPtr.GetPtrGeneration()).Cast<GameObject>())
	{
		// 이미 파괴되었거나 이미 DontDestroyOnLoad 씬에 있으면 처리하지 않음
		if (go->IsDestroyed() || go->GetScene().id_DDOL)
			return;

		// 부모가 있는 경우 부모를 끊기
		go->GetTransform()->SetParent(nullptr);

		std::vector<ObjPtr<GameObject>> gameObjectsToProcess;
		gameObjectsToProcess.push_back(go);

		// BFS (너비 우선 탐색) 방식으로 계층 구조를 순회하여 스택 오버플로우 방지
		while (!gameObjectsToProcess.empty())
		{
			ObjPtr<GameObject> currentGo = gameObjectsToProcess.back();
			gameObjectsToProcess.pop_back();

			// 이미 처리했거나 파괴되었거나 DontDestroyOnLoad 씬에 있으면 건너뜀
			if (currentGo->IsDestroyed() || currentGo->GetScene().id_DDOL)
				continue;

			// 자신을 현재 씬에서 해제하고 DontDestroyOnLoad 씬에 등록
			if (auto sceneRaw = SceneManager::Get().GetSceneRaw(currentGo->GetScene())) // 씬이 유효한지 확인
			{
				sceneRaw->UnRegisterGameObject(currentGo);
			}
			SceneManager::Get().RegisterGameObjectToDDOL(currentGo);

			// 자식들을 큐에 추가하여 다음 반복에서 처리
			for (size_t i = 0; i < currentGo->GetTransform()->GetChildCount(); ++i)
			{
				if (auto childGo = currentGo->GetTransform()->GetChild(i)->GetGameObject())
				{
					gameObjectsToProcess.push_back(childGo);
				}
			}
		}
	}
}

void MMMEngine::Object::Destroy(const ObjPtrBase& objPtr, float delay)
{
	if (ObjectManager::Get().GetPtr<Object>(objPtr.GetPtrID(), objPtr.GetPtrGeneration()).Cast<Transform>())
	{
#ifdef _DEBUG
		assert(false && "Transform은 파괴할 수 없습니다.");
#endif
		return;
	}

	ObjectManager::Get().Destroy(objPtr, delay);
}
