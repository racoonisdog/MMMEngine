#include "SceneSerializer.h"
#include "GameObject.h"
#include "Component.h"
#include "StringHelper.h"
#include "rttr/type"
#include "Transform.h"

#include <fstream>
#include <filesystem>

DEFINE_SINGLETON(MMMEngine::SceneSerializer)

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace MMMEngine;
using namespace rttr;

std::unordered_map<std::string, rttr::variant> g_objectTable;

json SerializeVariant(const rttr::variant& var);
json SerializeObject(const rttr::instance& obj)
{
    json j;
    type t = obj.get_type();

    for (auto& prop : t.get_properties(
        rttr::filter_item::instance_item |
        rttr::filter_item::public_access |
        rttr::filter_item::non_public_access))
    {
        if (prop.is_readonly())
            continue;
        rttr::variant value = prop.get_value(obj);
        j[prop.get_name().to_string()] = SerializeVariant(value);
    }

    return j;
}

json SerializeVariant(const rttr::variant& var)
{
    rttr::type t = var.get_type();

    if (t.is_arithmetic())
    {
        if (t == type::get<bool>()) return var.to_bool();
        if (t == type::get<int>()) return var.to_int();
        if (t == type::get<unsigned int>()) return var.to_uint32();
        if (t == type::get<long long>()) return var.to_int64();
        if (t == type::get<uint64_t>()) return var.to_uint64(); // 핵심: uint64_t 추가
        if (t == type::get<float>()) return var.to_float();
        if (t == type::get<double>()) return var.to_double();
    }

    if (t == type::get<MMMEngine::Utility::MUID>()) {
        return var.get_value<MMMEngine::Utility::MUID>().ToString();
    }

    if (t == type::get<std::string>())
    {
        return var.to_string();
    }

    if (t.is_sequential_container())
    {
        json arr = json::array();
        auto view = var.create_sequential_view();
        for (const auto& item : view)
        {
            arr.push_back(SerializeVariant(item));
        }
        return arr;
    }

    if (var.get_type().get_name().to_string().find("ObjPtr") != std::string::npos)
    {
        MMMEngine::Object* obj = nullptr;
        if (var.convert(obj) && obj != nullptr)
        {
            return obj->GetMUID().ToString();
        }
        return nullptr;
    }

    if (t.is_associative_container())
    {
        json obj;
        auto view = var.create_associative_view();
        for (auto& item : view)
        {
            obj[SerializeObject(item.first).dump()] = SerializeObject(item.second);
        }
        return obj;
    }
    //Todo : 정의
    // 사용자 정의 타입 -> 재귀
    return SerializeObject(var);
}

json SerializeComponent(const ObjPtr<Component>& comp)
{
	json compJson;
	type type = type::get(*comp);
	compJson["Type"] = type.get_name().to_string();

	for (auto& prop : type.get_properties(
        rttr::filter_item::instance_item |
        rttr::filter_item::public_access |
        rttr::filter_item::non_public_access))
	{
        auto string = prop.get_name().to_string();
        if (prop.is_readonly())
            continue;

		rttr::variant value = prop.get_value(*comp);
		compJson["Props"][prop.get_name().to_string()] = SerializeVariant(value);
	}

	return compJson;
}

void MMMEngine::SceneSerializer::Serialize(const Scene& scene, std::wstring path)
{
	json snapshot;

    auto sceneMUID = scene.GetMUID().IsEmpty() ? Utility::MUID::NewMUID() : scene.GetMUID();

	snapshot["MUID"] = sceneMUID.ToString();
    snapshot["Name"] = scene.GetName();
        //Utility::StringHelper::WStringToString(Utility::StringHelper::ExtractFileName(path));

	json goArray = json::array();

	for (auto& goPtr : scene.m_gameObjects)
	{
		if (!goPtr.IsValid())
			continue;

		json goJson;
		goJson["Name"] = goPtr->GetName();
		goJson["MUID"] = goPtr->GetMUID().ToString();    
        goJson["Layer"] = goPtr->GetLayer();
        goJson["Tag"] = goPtr->GetTag();
        goJson["Active"] = goPtr->IsActiveSelf();

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

    fs::path p(path);
    if (p.has_parent_path() && !fs::exists(p.parent_path())) {
        fs::create_directories(p.parent_path());
    }

    std::ofstream file(path, std::ios::binary);
    if (!file.is_open()) {
        throw std::runtime_error("파일을 열 수 없습니다: " + Utility::StringHelper::WStringToString(path));
    }

    file.write(reinterpret_cast<const char*>(v.data()), v.size());
    file.close();
}


void DeserializeVariant(rttr::variant& target, const json& j, type target_type);

void DeserializeObject(rttr::instance obj, const json& j)
{
    type t = obj.get_type();

    for (auto& prop : t.get_properties(
        rttr::filter_item::instance_item |
        rttr::filter_item::public_access |
        rttr::filter_item::non_public_access))
    {
        if (prop.is_readonly())
            continue;

        std::string propName = prop.get_name().to_string();
        if (!j.contains(propName))
            continue;

        rttr::variant currentValue = prop.get_value(obj);
        DeserializeVariant(currentValue, j[propName], prop.get_type());
        prop.set_value(obj, currentValue);
    }
}

void DeserializeVariant(rttr::variant& target, const json& j, type target_type)
{
    if (j.is_null())
    {
        target = rttr::variant();
        return;
    }

    if (target_type.is_arithmetic())
    {
        if (target_type == type::get<bool>()) target = j.get<bool>();
        else if (target_type == type::get<int>()) target = j.get<int>();
        else if (target_type == type::get<unsigned int>()) target = j.get<unsigned int>();
        else if (target_type == type::get<long long>()) target = j.get<long long>();
        else if (target_type == type::get<uint64_t>()) target = j.get<uint64_t>();
        else if (target_type == type::get<float>()) target = j.get<float>();
        else if (target_type == type::get<double>()) target = j.get<double>();
        return;
    }

    if (target_type == type::get<MMMEngine::Utility::MUID>())
    {
        std::string muidStr = j.get<std::string>();
        target = MMMEngine::Utility::MUID::Parse(muidStr);
        return;
    }

    if (target_type == type::get<std::string>())
    {
        target = j.get<std::string>();
        return;
    }

    if (target_type.is_sequential_container())
    {
        auto view = target.create_sequential_view();
        view.clear();

        // 템플릿 인자 가져오기
        auto args = target_type.get_wrapped_type().get_template_arguments();
        auto it = args.begin();
        if (it == args.end())
            return;

        type value_type = *it;

        for (const auto& item : j)
        {
            rttr::variant element = value_type.create();
            DeserializeVariant(element, item, value_type);
            view.insert(view.end(), element);
        }
        return;
    }

    if (target_type.is_associative_container())
    {
        auto view = target.create_associative_view();
        view.clear();

        // 템플릿 인자 가져오기 (key, value)
        auto args = target_type.get_wrapped_type().get_template_arguments();
        auto it = args.begin();
        if (it == args.end())
            return;

        type key_type = *it;
        ++it;
        if (it == args.end())
            return;

        type value_type = *it;

        for (auto& [key, value] : j.items())
        {
            rttr::variant k = key_type.create();
            rttr::variant v = value_type.create();

            json keyJson = json::parse(key);
            DeserializeObject(k, keyJson);
            DeserializeObject(v, value);

            view.insert(k, v);
        }
        return;
    }

    // ObjPtr<T> 타입 처리
    if (target_type.get_name().to_string().find("ObjPtr") != std::string::npos)
    {
        std::string muidStr = j.get<std::string>();

        auto it = g_objectTable.find(muidStr);
        if (it != g_objectTable.end())
        {
            // 변환 시도하지 말고 그대로 대입
            target = it->second;
        }
        return;
    }

    //Todo: desc정의 
    // 사용자 정의 객체
    if (!target.is_valid() || target.get_type() != target_type)
    {
        target = target_type.create();
    }

    DeserializeObject(target, j);
}

ObjPtr<Component> DeserializeComponent(const json& compJson)
{
    std::string typeName = compJson["Type"].get<std::string>();
    type compType = type::get_by_name(typeName);

    if (!compType.is_valid())
        return nullptr;

    rttr::variant compVariant = compType.create();
    ObjPtr<Component> comp = nullptr;

    // variant에서 ObjPtr<Component>로 변환
    if (compVariant.can_convert<ObjPtr<Component>>())
    {
        comp = compVariant.convert<ObjPtr<Component>>();
    }

    if (!comp.IsValid())
        return nullptr;

    // Component의 MUID를 먼저 테이블에 등록
    const json& props = compJson["Props"];
    if (props.contains("MUID"))
    {
        std::string muid = props["MUID"].get<std::string>();
        g_objectTable[muid] = comp;
    }

    // 속성 복원 (ObjPtr도 바로 처리됨)
    DeserializeObject(*comp, props);

    return comp;
}

void DeserializeTransform(Transform& tr, const json& j)
{
    type t = type::get<Transform>();

    for (auto& prop : t.get_properties(
        rttr::filter_item::instance_item |
        rttr::filter_item::public_access |
        rttr::filter_item::non_public_access))
    {
        if (prop.is_readonly())
            continue;

        std::string name = prop.get_name().to_string();
        if (name == "Parent" || name == "m_parent" || name == "MUID" || name == "m_muid")
            continue;

        if (!j.contains(name))
            continue;

        rttr::variant v = prop.get_value(tr);
        DeserializeVariant(v, j[name], prop.get_type());
        prop.set_value(tr, v);
    }
}

static const json* FindTransformComp(const json& components)
{
    for (const auto& c : components)
    {
        if (!c.contains("Type")) continue;
        std::string t = c["Type"].get<std::string>();
        if (t == "Transform") return &c;
    }
    return nullptr;
}

void MMMEngine::SceneSerializer::Deserialize(Scene& scene, const SnapShot& snapshot)
{
    g_objectTable.clear();
    std::unordered_map<std::string, std::string> pendingParent; // childTrMUID -> parentTrMUID

    // Scene MUID
    if (auto parsed = Utility::MUID::Parse(snapshot["MUID"].get<std::string>()); parsed.has_value())
        scene.SetMUID(parsed.value());

    // Scene Name
    scene.SetName(snapshot["Name"].get<std::string>());

    const json& gameObjects = snapshot["GameObjects"];

    // 1-pass: GO + Transform(기존) MUID/값 복원 + 테이블 등록
    for (const auto& goJson : gameObjects)
    {
        std::string goName = goJson["Name"].get<std::string>();
        std::string goMUID = goJson["MUID"].get<std::string>();
        uint32_t goLayer = goJson["Layer"].get<uint32_t>();
        std::string goTag = goJson["Tag"].get<std::string>();
        bool active = true;
        if (goJson.contains("Active"))
        {
            active = goJson["Active"].get<bool>();
        }

        ObjPtr<GameObject> go = scene.CreateGameObject(goName);
        scene.RegisterGameObject(go);
        go->SetName(goName);
        go->SetLayer(goLayer);
        go->SetTag(goTag);
        go->SetActive(active);

        if (auto parsedGo = Utility::MUID::Parse(goMUID); parsedGo.has_value())
            go->SetMUID(parsedGo.value());

        g_objectTable[goMUID] = go;

        // Transform json 찾기
        const json& components = goJson["Components"];
        const json* trComp = FindTransformComp(components);
        if (!trComp || !trComp->contains("Props"))
            continue; // 또는 throw

        const json& trProps = (*trComp)["Props"];

        // 기존 Transform 가져오기
        auto tr = go->GetTransform();

        // Transform MUID는 Props["MUID"]
        std::string trMUID = trProps["MUID"].get<std::string>();
        if (auto parsedTr = Utility::MUID::Parse(trMUID); parsedTr.has_value())
            tr->SetMUID(parsedTr.value());

        g_objectTable[trMUID] = tr;

        // Transform 값 복원 (Parent/MUID는 스킵)
        DeserializeTransform(*tr, trProps);

        // Parent는 나중에
        if (trProps.contains("Parent") && !trProps["Parent"].is_null())
            pendingParent[trMUID] = trProps["Parent"].get<std::string>();
    }

    // 2-pass: 일반 컴포넌트 생성/복원 (Transform은 제외 + RectTransform도 제외)
    for (const auto& goJson : gameObjects)
    {
        std::string goMUID = goJson["MUID"].get<std::string>();
        auto itGo = g_objectTable.find(goMUID);
        if (itGo == g_objectTable.end()) continue;

        ObjPtr<GameObject> go = itGo->second.get_value<ObjPtr<GameObject>>();

        const json& components = goJson["Components"];
        for (const auto& compJson : components)
        {
            std::string typeName = compJson["Type"].get<std::string>();
            if (typeName == "Transform") // 정확 일치로 스킵 권장
                continue;

            ObjPtr<Component> comp = DeserializeComponent(compJson);
            if (comp.IsValid())
            {
                comp->m_gameObject = go;
                go->RegisterComponent(comp);
                comp->Initialize();
            }
        }
    }

    // 2.5-pass: RectTransform만 찾아서 값타입만 역직렬화 + pendingRectParent에 기록해두기

    // 3-pass: Parent 연결 (Transform MUID 기준)
    for (auto& [childTrMUID, parentTrMUID] : pendingParent)
    {
        auto itChild = g_objectTable.find(childTrMUID);
        auto itParent = g_objectTable.find(parentTrMUID);

        if (itChild == g_objectTable.end() || itParent == g_objectTable.end())
            continue; // 또는 로그

        auto childTr = itChild->second.get_value<ObjPtr<Transform>>();
        auto parentTr = itParent->second.get_value<ObjPtr<Transform>>();
        childTr->SetParent(parentTr, false);
    }

    g_objectTable.clear();
}

void MMMEngine::SceneSerializer::SerializeToMemory(const Scene& scene, SnapShot& snapshot)
{
    auto sceneMUID = scene.GetMUID().IsEmpty() ? Utility::MUID::NewMUID() : scene.GetMUID();

    snapshot["MUID"] = sceneMUID.ToString();
    snapshot["Name"] = scene.GetName();
    //Utility::StringHelper::WStringToString(Utility::StringHelper::ExtractFileName(path));

    json goArray = json::array();

    for (auto& goPtr : scene.m_gameObjects)
    {
        if (!goPtr.IsValid())
            continue;

        json goJson;
        goJson["Name"] = goPtr->GetName();
        goJson["MUID"] = goPtr->GetMUID().ToString();
        goJson["Layer"] = goPtr->GetLayer();
        goJson["Tag"] = goPtr->GetTag();
        goJson["Active"] = goPtr->IsActiveSelf();

        json compArray = json::array();
        for (auto& comp : goPtr->GetAllComponents()) // 컴포넌트 리스트 가정
        {
            compArray.push_back(SerializeComponent(comp));
        }
        goJson["Components"] = compArray;

        goArray.push_back(goJson);
    }

    snapshot["GameObjects"] = goArray;
}

/// <summary>
/// 최적화된 단일경로용 바이너리 파일을 만들어줍니다.
/// </summary>
/// <param name="scenes"></param>
/// <param name="rootPath"></param>
void MMMEngine::SceneSerializer::ExtractScenesList(const std::vector<Scene*>& scenes,
    const std::wstring& rootPath)
{
    namespace fs = std::filesystem;

    nlohmann::json sceneListJson = nlohmann::json::array();

    fs::path root(rootPath);

    for (size_t i = 0; i < scenes.size(); ++i)
    {
        Scene* scene = scenes[i];
        if (!scene) continue;

        // "custom1.scene"
        const std::string sceneFileName = scene->GetName() + ".scene";

        nlohmann::json sceneEntry;
        sceneEntry["index"] = i;
        sceneEntry["filepath"] = sceneFileName; // 리스트에는 상대경로(파일명)만 저장
        sceneListJson.push_back(sceneEntry);
    }

    // rootpath/sceneList.bin
    fs::path listFullPath = root / L"sceneList.bin";

    std::ofstream outFile(listFullPath, std::ios::binary);
    if (outFile)
    {
        std::vector<uint8_t> msgpack = nlohmann::json::to_msgpack(sceneListJson);
        outFile.write(reinterpret_cast<const char*>(msgpack.data()),
            static_cast<std::streamsize>(msgpack.size()));
    }
}
