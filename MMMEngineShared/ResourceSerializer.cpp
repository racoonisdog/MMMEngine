#include "ResourceSerializer.h"
#include "json/json.hpp"
#include "StaticMesh.h"
#include "RenderShared.h"
#include "rttr/type"

#include <string>
#include <fstream>
#include "StringHelper.h"
#include "MaterialSerializer.h"

DEFINE_SINGLETON(MMMEngine::ResourceSerializer);

namespace fs = std::filesystem;
using json = nlohmann::json;
using namespace MMMEngine;
using namespace rttr;


json SerializeMeshVariant(const rttr::variant& var) {
	type t = var.get_type();

	if (t.is_arithmetic()) {
		if (t == type::get<bool>()) return var.to_bool();
		if (t == type::get<int>()) return var.to_int();
		if (t == type::get<unsigned int>()) return var.to_uint32();
		if (t == type::get<long long>()) return var.to_int64();
		if (t == type::get<uint64_t>()) return var.to_uint64();
		if (t == type::get<float>()) return var.to_float();
		if (t == type::get<double>()) return var.to_double();
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
			if (!item.is_valid())
				continue;

			json val = SerializeMeshVariant(item);

			// null 값은 추가하지 않음
			if (!val.is_null())
				arr.push_back(val);
		}
		return arr;
	}

	if (t.is_class()) {
		// Vector2 → 배열 [x,y]
        if (t == type::get<DirectX::SimpleMath::Vector2>()) {
            auto vec = var.get_value<DirectX::SimpleMath::Vector2>();
            return json::array({ vec.x, vec.y });
        }
        // Vector3 → 배열 [x,y,z]
        if (t == type::get<DirectX::SimpleMath::Vector3>()) {
            auto vec = var.get_value<DirectX::SimpleMath::Vector3>();
            return json::array({ vec.x, vec.y, vec.z });
        }

		json obj;
		for (auto& prop : t.get_properties(
			rttr::filter_item::instance_item |
			rttr::filter_item::public_access |
			rttr::filter_item::non_public_access))
		{
			rttr::variant value = prop.get_value(var);
			if (value.is_valid()) {
				json val = SerializeMeshVariant(value);
				if (!val.is_null())
					obj[prop.get_name().to_string()] = SerializeMeshVariant(value);
			}
		}
		return obj;
	}

	return {};
}

json SerializeVertex(const std::vector<Mesh_Vertex>& _vertices)
{
	json meshJson;
	type subt = type::get(_vertices);

	for (auto& vertex : _vertices)
	{
		type vert = type::get(vertex);
		json vertJson;

		for (auto& prop : vert.get_properties(
			rttr::filter_item::instance_item |
			rttr::filter_item::public_access |
			rttr::filter_item::non_public_access))
		{
			if (prop.is_readonly())
				continue;

			rttr::variant var = prop.get_value(vertex);
			vertJson[prop.get_name().to_string()] = SerializeMeshVariant(var);
		}
		meshJson.push_back(vertJson);
	}

	return meshJson;
}

json SerializeMesh(const MeshData& _meshData)
{
	json meshJson;

	json vertexJson = json::array();
	for (auto& vSubMesh : _meshData.vertices) {
		vertexJson.push_back(SerializeVertex(vSubMesh));
	}

	json indexJson = json::array();
	for (const auto& iSubMesh : _meshData.indices) {
		json arr = json::array();
		for (UINT val : iSubMesh)
			arr.push_back(val);
		indexJson.push_back(arr);
	}
	
	meshJson["Vertices"] = vertexJson;
	meshJson["Indices"] = indexJson;

	return meshJson;
}

json SerializeMeshGroup(const std::unordered_map<UINT, std::vector<UINT>>& meshGroupData)
{
	json out;
	type subt = type::get(meshGroupData);
	for (const auto& [mat, meshArr] : meshGroupData) {
		out[std::to_string(mat)] = meshArr;
	}

	return out;
}

fs::path MMMEngine::ResourceSerializer::Serialize_StaticMesh(const StaticMesh* _in, std::wstring _path, std::wstring _name)
{
	json snapshot;

	auto meshMUID = _in->GetMUID().IsEmpty() ? Utility::MUID::NewMUID() : _in->GetMUID();

	snapshot["MUID"] = meshMUID.ToString();

	json matJson = json::array();
	int index = 0;
	for (auto& matPtr : _in->materials) {
		fs::path matPath = MaterialSerializer::Get().Serealize(matPtr.get(), _path, _name, index);
		++index;

		matJson.push_back(matPath.u8string());
	}
	snapshot["Materials"] = matJson;

	json meshJson = json::array();
	meshJson.push_back(SerializeMesh(_in->meshData));
	snapshot["Mesh"] = meshJson;

	json meshgroupJson;
	meshgroupJson.push_back(SerializeMeshGroup(_in->meshGroupData));
	snapshot["MeshGroup"] = meshgroupJson;

	// Json 출력
	std::vector<uint8_t> v = json::to_msgpack(snapshot);

	fs::path p(ResourceManager::Get().GetCurrentRootPath());
	p = p / _path;
	p = p / (_name.append(L"_StaticMesh.staticmesh"));

	if (p.has_parent_path() && !fs::exists(p.parent_path())) {
		fs::create_directories(p.parent_path());
	}

	std::ofstream file(p.string(), std::ios::binary);
	//std::ofstream file(p.string());
	if (!file.is_open()) {
		throw std::runtime_error("파일을 열 수 없습니다: " + Utility::StringHelper::WStringToString(_path));
	}

	/*file << snapshot.dump(4);
	file.close();*/

	file.write(reinterpret_cast<const char*>(v.data()), v.size());
	file.close();

	return p;
}

void MMMEngine::ResourceSerializer::DeSerialize_StaticMesh(StaticMesh* _out, std::wstring _path)
{
	//// 파일 열기
	//std::ifstream file(_path, std::ios::binary);
	//if (!file.is_open())
	//{
	//	throw std::runtime_error("파일을 열 수 없습니다: " + Utility::StringHelper::WStringToString(_path));
	//}

	//// 파일 전체 읽기
	//std::vector<uint8_t> buffer((std::istreambuf_iterator<char>(file)),
	//	std::istreambuf_iterator<char>());
	std::ifstream file(_path, std::ios::binary | std::ios::ate);
	auto size = file.tellg();
	file.seekg(0, std::ios::beg);

	std::vector<char> buffer(size);
	file.read(buffer.data(), size);
	file.close();

	// msgpack → json 변환
	json snapshot = json::from_msgpack(buffer);

	// MUID 복원 ( 아직 안씀)
	/*if (snapshot.contains("MUID"))
	{
		const_cast<StaticMesh*>(_out)->SetMUID(muid);ss
	}*/

	// Materials 복원
	if (snapshot.contains("Materials"))
	{
		auto& matJson = snapshot["Materials"];
		std::vector<ResPtr<Material>> mats;
		for (auto& m : matJson)
		{
			fs::path basePath(ResourceManager::Get().GetCurrentRootPath());
			fs::path matPath(_path);
			matPath = matPath.parent_path();
			matPath = matPath / m.get<std::string>();

			matPath = matPath.lexically_relative(basePath);

			auto mat = std::make_shared<Material>();
			mat = ResourceManager::Get().Load<Material>(matPath.wstring());
			mats.push_back(mat);
		}
		_out->materials = std::move(mats);
	}

	// Mesh 복원
	if (snapshot.contains("Mesh")) {
		auto& meshJsonArr = snapshot["Mesh"];
		if (!meshJsonArr.empty()) {
			auto& meshJson = meshJsonArr[0];
			MeshData meshData;

			// Vertices 복원
			if (meshJson.contains("Vertices")) {
				for (auto& subMeshJson : meshJson["Vertices"]) {
					std::vector<Mesh_Vertex> subMesh;
					for (auto& vertJson : subMeshJson) {
						Mesh_Vertex vertex;

						// RTTR 파싱
						//type vertType = type::get<Mesh_Vertex>();
						//for (auto& prop : vertType.get_properties()) {
						//	if (prop.is_readonly())
						//		continue;

						//	std::string name = prop.get_name().to_string();
						//	if (!vertJson.contains(name))
						//		continue;

						//	auto& jval = vertJson[name];
						//	rttr::variant newVal;

						//	// 숫자/문자/배열 처리
						//	if (jval.is_number_integer())
						//		newVal = jval.get<int>();
						//	else if (jval.is_number_float())
						//		newVal = jval.get<float>();
						//	else if (jval.is_string())
						//		newVal = jval.get<std::string>();
						//	else if (jval.is_array() && jval.size() == 3) {
						//		DirectX::SimpleMath::Vector3 vec;
						//		vec.x = jval[0].get<float>();
						//		vec.y = jval[1].get<float>();
						//		vec.z = jval[2].get<float>();
						//		newVal = vec;
						//	}
						//	else if (jval.is_array() && jval.size() == 2) {
						//		DirectX::SimpleMath::Vector2 vec;
						//		vec.x = jval[0].get<float>();
						//		vec.y = jval[1].get<float>();
						//		newVal = vec;
						//	}
						//	else if (jval.is_array()) {
						//		std::vector<int> arr;
						//		for (auto& elem : jval)
						//			arr.push_back(elem.get<int>());
						//		newVal = arr;
						//	}

						//	if (newVal.is_valid())
						//		prop.set_value(vertex, newVal);
						//}

						// 직접 파싱
						vertex.Pos = { vertJson["Pos"][0], vertJson["Pos"][1], vertJson["Pos"][2] };
						vertex.Normal = { vertJson["Normal"][0], vertJson["Normal"][1], vertJson["Normal"][2] };
						vertex.Tangent = { vertJson["Tangent"][0], vertJson["Tangent"][1], vertJson["Tangent"][2] };
						vertex.UV = { vertJson["UV"][0], vertJson["UV"][1] };
						subMesh.push_back(vertex);
					}
					meshData.vertices.push_back(subMesh);
				}
			}

			// Indices 복원
			if (meshJson.contains("Indices")) {
				for (auto& iSubMeshJson : meshJson["Indices"]) {
					std::vector<UINT> indices;
					for (auto& elem : iSubMeshJson) {
						indices.push_back(elem.get<UINT>());
					}
					meshData.indices.push_back(indices);
				}
			}

			_out->meshData = std::move(meshData);
		}
	}


	// MeshGroup 복원
	if (snapshot.contains("MeshGroup"))
	{
		auto& meshGroupJsonArr = snapshot["MeshGroup"];
		if (!meshGroupJsonArr.empty()) {
			auto& meshGroupJson = meshGroupJsonArr[0];
			std::unordered_map<UINT, std::vector<UINT>> data;
			for (auto& [key, value] : meshGroupJson.items()) {
				if (key == "Type") continue;
				UINT matId = static_cast<UINT>(std::stoul(key));
				data[matId] = value.get<std::vector<UINT>>();
			}
			_out->meshGroupData = std::move(data);
		}
	}
}
