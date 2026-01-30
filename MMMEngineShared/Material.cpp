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
		.property("VShader", &Material::GetVShaderRttr, &Material::SetVShader)
		.property("PShader", &Material::GetPShaderRttr, &Material::SetPShader);

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
}


// 프로퍼티 생성
void MMMEngine::Material::AddProperty(const std::wstring _name, const PropertyValue& _value)
{
	// 없으면 생성, 있으면 갱신
	m_properties[_name] = _value;
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

void MMMEngine::Material::SetVShader(const std::wstring& _filePath)
{
	if (_filePath.empty())
		return;
	m_pVShader = ResourceManager::Get().Load<VShader>(_filePath);
}

void MMMEngine::Material::SetPShader(const std::wstring& _filePath)
{
	if (_filePath.empty())
		return;
	m_pPShader = ResourceManager::Get().Load<PShader>(_filePath);
}

const MMMEngine::ResPtr<MMMEngine::VShader> MMMEngine::Material::GetVShader()
{
	return m_pVShader;
}

const MMMEngine::ResPtr<MMMEngine::PShader> MMMEngine::Material::GetPShader()
{
	return m_pPShader;
}

const std::wstring& MMMEngine::Material::GetVShaderRttr()
{
	return m_pVShader->GetFilePath();
}

const std::wstring& MMMEngine::Material::GetPShaderRttr()
{
	return m_pPShader->GetFilePath();
}

void MMMEngine::Material::LoadTexture(const std::wstring& _propertyName, const std::wstring& _filePath)
{
	auto texture = ResourceManager::Get().Load<Texture2D>(_filePath);

	SetProperty(_propertyName, texture);
}

bool MMMEngine::Material::LoadFromFilePath(const std::wstring& _filePath)
{
	MaterialSerializer::Get().UnSerealize(this, _filePath);

	// 타입에 따라 프로퍼티 생성, 삭제
	auto type = ShaderInfo::Get().GetShaderType(m_pPShader->GetFilePath());
	ShaderInfo::Get().ConvertMaterialType(type, this);

	return true;
}
