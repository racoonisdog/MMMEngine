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

namespace fs = std::filesystem;
namespace mw = Microsoft::WRL;
using json = nlohmann::json;

RTTR_REGISTRATION
{
	using namespace rttr;

	registration::class_<MMMEngine::Material>("Material")
		.constructor<>()(rttr::policy::ctor::as_std_shared_ptr)
		.property_readonly("VShader", &MMMEngine::Material::GetVShader)
		.property_readonly("PShader", &MMMEngine::Material::GetPShader)
		.method("GetProperty", &MMMEngine::Material::GetProperty)
		.method("SetProperty", &MMMEngine::Material::SetProperty);
}


// 프로퍼티 설정
void MMMEngine::Material::SetProperty(const std::wstring& _name, const MMMEngine::PropertyValue& value)
{
	m_properties[_name] = value;
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

void MMMEngine::Material::LoadTexture(const std::wstring& _propertyName, const std::wstring& _filePath)
{
	auto texture = ResourceManager::Get().Load<Texture2D>(_filePath);

	SetProperty(_propertyName, texture);
}

bool MMMEngine::Material::LoadFromFilePath(const std::wstring& _filePath)
{
	MaterialSerializer::Get().UnSerealize(this, _filePath);

	if (!m_pVShader)
		m_pVShader = RenderManager::Get().m_pDefaultVSShader;
	if (!m_pPShader)
		m_pPShader = RenderManager::Get().m_pDefaultPSShader;

	return true;
}
