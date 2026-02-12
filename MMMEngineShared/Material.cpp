

#include "Material.h"
#include "VShader.h"
#include "PShader.h"
#include "Texture2D.h"
#include "ResourceManager.h"
#include "RenderManager.h"
#include <DirectXTex.h>
#include <WICTextureLoader.h>
#include <RendererTools.h>
#include "json/json.hpp"

#include <rttr/registration>
#include "MaterialSerializer.h"
#include "ShaderInfo.h"

namespace fs = std::filesystem;
namespace mw = Microsoft::WRL;
using json = nlohmann::json;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Material>("Material")
		.constructor<>()(policy::ctor::as_std_shared_ptr)
		.property("VShader", &Material::GetVShader, &Material::SetVShader)
		.property("PShader", &Material::GetPShader, &Material::SetPShader);

	type::register_converter_func(
		[](std::shared_ptr<Resource> from, bool& ok) -> std::shared_ptr<Material>
		{
			if (!from) {  // nullptr 허용
				ok = true;
				return nullptr;
			}
			
			auto result = std::dynamic_pointer_cast<Material>(from);
			ok = (result != nullptr);
			return result;
		}
	);

	//type::register_converter_func(
	//	[](std::shared_ptr<Material> from, bool& ok) -> std::shared_ptr<Resource>
	//	{
	//		ok = true; // nullptr도 허용
	//		return std::static_pointer_cast<Resource>(from);
	//	}
	//);

}


// 프로퍼티 생성
void MMMEngine::Material::AddProperty(const std::wstring _name, const PropertyValue& _value)
{
	// 있으면 타입 비교후 갱신
	auto it = m_properties.find(_name);
	if (it != m_properties.end()) {
		if(_value.index() == it->second.index())
			it->second = _value;
	}
	else {
		// 없으면 생성, 있으면 갱신
		m_properties[_name] = _value;
	}
}

// 프로퍼티 갱신
void MMMEngine::Material::SetProperty(const std::wstring _name, const MMMEngine::PropertyValue& _value)
{
	auto it = m_properties.find(_name);
	if (it == m_properties.end())
		return;

	// 같은 타입이면 갱신
	if (_value.index() == it->second.index()) {
		it->second = _value;
	}
}

// 프로퍼티 삭제
void MMMEngine::Material::RemoveProperty(const std::wstring& _name)
{
	auto it = m_properties.find(_name);
	if (it != m_properties.end()) {
		m_properties.erase(it);
	}
}

// 프로퍼티 가져오기
MMMEngine::PropertyValue MMMEngine::Material::GetProperty(const std::wstring& _name) const
{
	auto it = m_properties.find(_name);
	if (it != m_properties.end())
		return it->second;

	// TODO :: 뒤에 name 추가하기
	throw std::runtime_error("Property not found");

}

void MMMEngine::Material::SetVShader(const ResPtr<VShader> _vShader)
{
	if (!_vShader)
		return;
	m_pVShader = _vShader;
}

void MMMEngine::Material::SetPShader(const ResPtr<PShader> _pShader)
{
	if (!_pShader)
		return;
	m_pPShader = _pShader;
}

MMMEngine::ResPtr<MMMEngine::VShader> MMMEngine::Material::GetVShader()
{
	return m_pVShader;
}

MMMEngine::ResPtr<MMMEngine::PShader> MMMEngine::Material::GetPShader()
{
	return m_pPShader;
}

void MMMEngine::Material::LoadTexture(const std::wstring& _propertyName, const std::wstring& _filePath)
{
	auto texture = ResourceManager::Get().Load<Texture2D>(_filePath);

	SetProperty(_propertyName, texture);
}

bool MMMEngine::Material::LoadFromFilePath(const std::wstring& _filePath)
{
	std::filesystem::path fPath{ _filePath };

	if (!std::filesystem::exists(fPath)) {
		std::cout << "Material::Files does not exist!!" << std::endl;
		return false;
	}

	MaterialSerializer::Get().DeSerialize(this, _filePath);
	int prevPropSize = (int)m_properties.size();

	// 타입에 따라 프로퍼티 생성, 삭제
	if (!m_pPShader) {
		m_pPShader = ShaderInfo::Get().GetDefaultPShader();
	}

	auto type = ShaderInfo::Get().GetShaderType(m_pPShader->GetFilePath());
	ShaderInfo::Get().ConvertMaterialType(type, this);
	int currPropSize = (int)m_properties.size();

	// 프로퍼티 변경 감지시 자동으로 재직렬화
	if (currPropSize != prevPropSize) {
		std::wstring fileName = fPath.filename().wstring();
		std::wstring parentPath = fPath.parent_path().wstring();

		MaterialSerializer::Get().Serialize(this, parentPath, fileName, -1);
	}

	return true;
}
