#include "SceneSerializer.h"
#include "GameObject.h"
#include "Component.h"
#include "StringHelper.h"
#include "rttr/type"
#include "Transform.h"

#include <fstream>
#include <filesystem>

DEFINE_SINGLETON(MMMEngine::SceneSerializer)

using json = nlohmann::json;
using namespace MMMEngine;
using namespace rttr;

json SerializeComponent(ObjPtr<Component> comp)
{
	json compJson;
	type type = type::get(*comp);
	compJson["Type"] = type.get_name().to_string();

	auto ssss = type.get_name().to_string();

	for (auto& prop : type.get_properties())
	{
		auto sss = prop.get_name();
		rttr::variant value = prop.get_value(*comp);

		if (value.get_type().get_name().to_string().find("ObjPtr") != std::string::npos)
		{
			MMMEngine::Object* obj = nullptr;
			if (value.convert(obj))   // Transform* -> Object*
			{
				// obj로 공통 처리
				std::string ss = "r5";

				auto duid = obj->GetMUID().ToString();

				auto s4 = 241410;
			}
		}


		auto vv = value.get_value<ObjPtr<Object>>();
		if (vv.IsValid())
		{
			auto rir = 550;
		}

		// 데이터 타입에 따른 JSON 변환 (간단한 예시)
		if (value.is_type<ObjPtr<Object>>()) {
			// ObjPtr인 경우 MUID만 기록
			auto v = value.get_value<ObjPtr<Object>>();
			auto ptrMUID = v->GetMUID(); // prop.get_type().get_method("GetRaw").invoke(v).get_value<Object*>()->GetMUID();
			compJson["Props"][prop.get_name().to_string()] = ptrMUID.ToString();
		}
		else if (value.is_type<float>()) {
			compJson["Props"][prop.get_name().to_string()] = value.get_value<float>();
		}
		// ... 필요한 타입들에 대해 추가
	}

	return compJson;
}

void MMMEngine::SceneSerializer::Serialize(const Scene& scene, std::wstring path)
{
	json snapshot;

	snapshot["MUID"] = scene.GetMUID().ToString();
	snapshot["Name"] = Utility::StringHelper::ExtractFileName(path);

	json goArray = json::array();

	for (auto& goPtr : scene.m_gameObjects)
	{
		if (!goPtr.IsValid())
			continue;

		json goJson;
		goJson["Name"] = goPtr->GetName();
		goJson["MUID"] = goPtr->GetMUID().ToString();

		json compArray = json::array();
		for (auto& comp : goPtr->GetAllComponents()) // 컴포넌트 리스트 가정
		{
			compArray.push_back(SerializeComponent(comp));
		}
		goJson["Components"] = compArray;

		goArray.push_back(goJson);
	}

	snapshot["GameObjects"] = goArray;
	std::vector<uint8_t> v = json::to_msgpack(snapshot);


}
