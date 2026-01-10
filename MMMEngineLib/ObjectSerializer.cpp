#include "ObjectSerializer.h"
#include "Object.h"

json MMMEngine::ObjectSerializer::Serialize(const variant& objectHandle)
{
    json objJson;

    // ObjectPtr의 Get()으로 instance 얻기
    rttr::type handleType = objectHandle.get_type();
    auto getMethod = handleType.get_method("Get");

    if (!getMethod.is_valid())
        return objJson;

    variant getResult = getMethod.invoke(objectHandle);
    rttr::instance objInstance = getResult;

    if (!objInstance.is_valid())
        return objJson;

    rttr::type objType = objInstance.get_derived_type();

    objJson["type"] = objType.get_name().to_string();
    objJson["properties"] = json::object();

    // 모든 프로퍼티 직렬화
    for (auto& prop : objType.get_properties())
    {
        std::string propName = prop.get_name().to_string();
        variant value = prop.get_value(objInstance);

        if (auto jsonValue = SerializeVariant(value))
        {
            objJson["properties"][propName] = *jsonValue;
        }
    }

    return objJson;
}

std::optional<json> MMMEngine::ObjectSerializer::SerializeVariant(const variant& var)
{
    rttr::type t = var.get_type();

    // ObjPtr<T> -> 타입명 + GUID만 저장
    if (t.is_derived_from<ObjPtrBase>())
    {
        return SerializeObjectPtr(var);
    }

    // Sequential Container (std::vector 등)
    if (t.is_sequential_container())
    {
        return SerializeSequentialContainer(var);
    }

    // 기본형
    if (t.is_arithmetic())
    {
        json j;
        j["__type"] = t.get_name().to_string();
        j["__value"] = var.to_string();
        return j;
    }

    // std::string
    if (t == rttr::type::get<std::string>())
    {
        return var.get_value<std::string>();
    }

    // enum
    if (t.is_enumeration())
    {
        auto e = t.get_enumeration();
        return e.value_to_name(var).to_string();
    }

    // 사용자 정의 클래스 (Vector3, Quaternion 등)
    if (t.is_class())
    {
        json obj = json::object();
        rttr::instance inst = var;

        for (auto& prop : inst.get_derived_type().get_properties())
        {
            auto child = prop.get_value(inst);
            if (auto j = SerializeVariant(child))
                obj[prop.get_name().to_string()] = *j;
        }
        return obj;
    }

    return std::nullopt;
}

std::optional<json> MMMEngine::ObjectSerializer::SerializeObjectPtr(const variant& var)
{
    json j = json::object();

    rttr::type ptrType = var.get_type();
    j["__type"] = ptrType.get_name().to_string();  // "ObjPtr<GameObject>" 등

    // IsValid() 체크
    auto isValidMethod = ptrType.get_method("IsValid");
    if (!isValidMethod.is_valid())
        return j;

    variant isValidResult = isValidMethod.invoke(var);
    bool isValid = isValidResult.to_bool();

    if (!isValid)
    {
        j["__guid"] = nullptr;
        return j;
    }

    // Get() 호출하여 GUID 추출
    auto getMethod = ptrType.get_method("Get");
    if (!getMethod.is_valid())
        return j;

    variant getResult = getMethod.invoke(var);
    rttr::instance objInstance = getResult;

    if (!objInstance.is_valid())
    {
        j["__guid"] = nullptr;
        return j;
    }

    // GUID 프로퍼티 접근
    rttr::type objType = objInstance.get_derived_type();
    auto guidProp = objType.get_property("MUID");

    if (guidProp.is_valid())
    {
        variant guidVar = guidProp.get_value(objInstance);
        rttr::type guidType = guidVar.get_type();
        auto toStringMethod = guidType.get_method("ToString");

        if (toStringMethod.is_valid())
        {
            variant guidStrVar = toStringMethod.invoke(guidVar);
            j["__guid"] = guidStrVar.get_value<std::string>();
        }
    }
    else
    {
        j["__guid"] = nullptr;
    }

    return j;
}

std::optional<json> MMMEngine::ObjectSerializer::SerializeSequentialContainer(const variant& var)
{
    auto view = var.create_sequential_view();
    if (!view.is_valid())
        return std::nullopt;

    json arr = json::array();

    for (const auto& item : view)
    {
        if (auto j = SerializeVariant(item))
            arr.push_back(*j);
    }

    return arr;
}

bool MMMEngine::ObjectSerializer::Deserialize(const json& j, const variant& objectHandle)
{
    if (!j.contains("properties") || !j["properties"].is_object())
        return false;

    // ObjectPtr의 Get()으로 instance 얻기
    rttr::type handleType = objectHandle.get_type();
    auto getMethod = handleType.get_method("Get");

    if (!getMethod.is_valid())
        return false;

    variant getResult = getMethod.invoke(objectHandle);
    rttr::instance objInstance = getResult;

    if (!objInstance.is_valid())
        return false;

    rttr::type objType = objInstance.get_derived_type();

    // 모든 프로퍼티 역직렬화
    for (auto& prop : objType.get_properties())
    {
        std::string propName = prop.get_name().to_string();

        if (!j["properties"].contains(propName))
            continue;

        variant propValue = prop.get_value(objInstance);

        if (DeserializeVariant(j["properties"][propName], propValue))
        {
            prop.set_value(objInstance, propValue);
        }
    }

    return true;
}

bool MMMEngine::ObjectSerializer::DeserializeVariant(const json& j, variant& var)
{
    rttr::type t = var.get_type();

    // ObjPtr<T> -> null ObjectPtr만 생성 (GUID는 무시)
    if (t.is_derived_from<ObjPtrBase>())
    {
        return DeserializeObjectPtr(j, var);
    }

    // Sequential Container
    if (t.is_sequential_container())
    {
        return DeserializeSequentialContainer(j, var);
    }

    // 기본형
    if (j.is_object() && j.contains("__type") && j.contains("__value"))
    {
        std::string typeName = j["__type"];
        std::string valueStr = j["__value"];

        rttr::type storedType = rttr::type::get_by_name(typeName);
        if (!storedType.is_valid() || storedType != t)
            return false;

        variant tmp = valueStr;
        if (!tmp.convert(static_cast<type>(t)))
            return false;

        var = tmp;
        return true;
    }

    // std::string
    if (t == rttr::type::get<std::string>())
    {
        if (!j.is_string())
            return false;

        var = j.get<std::string>();
        return true;
    }

    // enum
    if (t.is_enumeration())
    {
        if (!j.is_string())
            return false;

        auto e = t.get_enumeration();
        auto v = e.name_to_value(j.get<std::string>());
        if (!v.is_valid())
            return false;

        var = v;
        return true;
    }

    // 사용자 정의 클래스
    if (t.is_class() && j.is_object())
    {
        rttr::instance inst = var;

        for (auto& prop : inst.get_derived_type().get_properties())
        {
            const auto name = prop.get_name().to_string();
            if (!j.contains(name))
                continue;

            variant child = prop.get_value(inst);
            if (DeserializeVariant(j[name], child))
            {
                prop.set_value(inst, child);
            }
        }
        return true;
    }

    return false;
}

bool MMMEngine::ObjectSerializer::DeserializeObjectPtr(const json& j, variant& var)
{
    if (!j.is_object() || !j.contains("__type"))
        return false;

    // null ObjPtr 생성 (나중에 SceneSerializer가 연결)
    rttr::type ptrType = var.get_type();
    variant nullPtr = ptrType.create();
    var = nullPtr;

    return true;
}

bool MMMEngine::ObjectSerializer::DeserializeSequentialContainer(const json& j, variant& containerVar)
{
    if (!j.is_array())
        return false;

    auto view = containerVar.create_sequential_view();
    if (!view.is_valid())
        return false;

    view.clear();

    rttr::type elementType = view.get_rank_type(1);

    for (const auto& itemJson : j)
    {
        variant element = elementType.create();

        if (DeserializeVariant(itemJson, element))
        {
            view.insert(view.end(), element);
        }
    }

    return true;
}
