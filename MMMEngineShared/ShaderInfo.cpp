#include "ShaderInfo.h"
#include <filesystem>
#include <d3dcompiler.h>

#include "VShader.h"
#include "PShader.h"

namespace fs = std::filesystem;

DEFINE_SINGLETON(MMMEngine::ShaderInfo);

Microsoft::WRL::ComPtr<ID3D11ShaderReflection> MMMEngine::ShaderInfo::MakeShaderReflection(ID3DBlob* _byteCode)
{
	Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;

	HR_T(D3DReflect(
		_byteCode->GetBufferPointer(),
		_byteCode->GetBufferSize(),
		IID_ID3D11ShaderReflection,
		(void**)reflection.GetAddressOf()));

	return reflection;
}

void MMMEngine::ShaderInfo::DeSerialize()
{
	//m_SRMap[ShaderType::PBR] = 
}

void MMMEngine::ShaderInfo::StartUp()
{
	// 텍스쳐 프로퍼티 타입정의
	m_typeInfo[S_PBR][L"basecolor"] = PropertyType::Texture;
	m_typeInfo[S_PBR][L"normal"] = PropertyType::Texture;
	m_typeInfo[S_PBR][L"emissive"] = PropertyType::Texture;
	m_typeInfo[S_PBR][L"shadowmap"] = PropertyType::Texture;
	m_typeInfo[S_PBR][L"opacity"] = PropertyType::Texture;
	m_typeInfo[S_PBR][L"metallic"] = PropertyType::Texture;
	m_typeInfo[S_PBR][L"roughness"] = PropertyType::Texture;
	m_typeInfo[S_PBR][L"ao"] = PropertyType::Texture;

	m_typeInfo[S_PBR][L"basecolor_factor"] = PropertyType::Constant;
	m_typeInfo[S_PBR][L"metallic_factor"] = PropertyType::Constant;
	m_typeInfo[S_PBR][L"roughness_factor"] = PropertyType::Constant;
	m_typeInfo[S_PBR][L"ao_factor"] = PropertyType::Constant;
	m_typeInfo[S_PBR][L"emissive_factor"] = PropertyType::Constant;

	// 텍스쳐 버퍼번호 하드코딩
	m_matPropertyMap[S_PBR][L"basecolor"] = 0;
	m_matPropertyMap[S_PBR][L"normal"] = 1;
	m_matPropertyMap[S_PBR][L"emissive"] = 2;
	m_matPropertyMap[S_PBR][L"shadowmap"] = 3;
	m_matPropertyMap[S_PBR][L"opacity"] = 4;
	m_matPropertyMap[S_PBR][L"metallic"] = 30;
	m_matPropertyMap[S_PBR][L"roughness"] = 31;
	m_matPropertyMap[S_PBR][L"ao"] = 32;

	// 상수버퍼이름 하드코딩
	m_matCBPropertyMap[S_PBR][L"basecolor_factor"] = L"S_PBR_materialbuffer";
	m_matCBPropertyMap[S_PBR][L"metallic_factor"] = L"S_PBR_materialbuffer";
	m_matCBPropertyMap[S_PBR][L"roughness_factor"] = L"S_PBR_materialbuffer";
	m_matCBPropertyMap[S_PBR][L"ao_factor"] = L"S_PBR_materialbuffer";
	m_matCBPropertyMap[S_PBR][L"emissive_factor"] = L"S_PBR_materialbuffer";

	// 상수버퍼번호 하드코딩
	m_CBIndexMap[S_PBR][L"S_PBR_materialbuffer"] = 3;

	m_CBBufferMap[L"pbr_materialbuffer"] = CreateConstantBuffer<Render_LightBuffer>();

	DeSerialize();
}

void MMMEngine::ShaderInfo::ShutDown()
{

}

const int MMMEngine::ShaderInfo::PropertyToIdx(const ShaderType _type, const std::wstring& _propertyName, PropertyType* _out /*= nullptr*/) const
{
	// ShaderType 존재 확인
	auto typeIt = m_typeInfo.find(_type);
	if (typeIt == m_typeInfo.end())
		return -1;

	// PropertyName 존재 확인
	auto propIt = typeIt->second.find(_propertyName);
	if (propIt == typeIt->second.end())
		return -1;

	// 타입 반환
	if (_out) *_out = propIt->second;

	// 텍스처면 tN 반환
	if (propIt->second == PropertyType::Texture) {
		auto texIt = m_matPropertyMap.at(_type).find(_propertyName);
		if (texIt == m_matPropertyMap.at(_type).end())
			return -1;
		return texIt->second;
	}
	// 상수면 bN 반환
	else if (propIt->second == PropertyType::Constant) {
		auto cbNameIt = m_matCBPropertyMap.at(_type).find(_propertyName);
		if (cbNameIt == m_matCBPropertyMap.at(_type).end())
			return -1;

		auto cbIndexIt = m_CBIndexMap.at(_type).find(cbNameIt->second);
		if (cbIndexIt == m_CBIndexMap.at(_type).end())
			return -1;

		return cbIndexIt->second;
	}

	return -1;

}
