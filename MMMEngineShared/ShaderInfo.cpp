#include "ShaderInfo.h"
#include <filesystem>
#include <d3dcompiler.h>

namespace fs = std::filesystem;

DEFINE_SINGLETON(MMMEngine::ShaderInfo);

void MMMEngine::ShaderInfo::CreateShaderReflection(std::wstring&& _filePath)
{
	// 타입 검색
	if (m_shaderTypeMap.find(_filePath) == m_shaderTypeMap.end())
		throw std::runtime_error("ShaderInfo::CreateShaderReflection : Shader Type not found !!");

	ShaderType _type = m_shaderTypeMap[_filePath];

	fs::path filePath (_filePath);
	if (!fs::exists(filePath))
		throw std::runtime_error("ShaderInfo::CreateShaderReflection : Shader File not found !!");

	auto res = ResourceManager::Get().Load<PShader>(_filePath);
	auto _byteCode = res->m_pBlob;

	Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflection;
	HR_T(D3DReflect(
		_byteCode->GetBufferPointer(),
		_byteCode->GetBufferSize(),
		IID_ID3D11ShaderReflection,
		(void**)reflection.GetAddressOf()));

	// 상수버퍼 개수 확인
	D3D11_SHADER_DESC shaderDesc;
	reflection->GetDesc(&shaderDesc);

	for (UINT i = 0; i < shaderDesc.ConstantBuffers; i++)
	{
		ID3D11ShaderReflectionConstantBuffer* cb = reflection->GetConstantBufferByIndex(i);
		D3D11_SHADER_BUFFER_DESC cbDesc;
		cb->GetDesc(&cbDesc);

		std::wstring cbName(cbDesc.Name, cbDesc.Name + strlen(cbDesc.Name));

		// 각 변수 정보 추출
		for (UINT v = 0; v < cbDesc.Variables; v++)
		{
			ID3D11ShaderReflectionVariable* var = cb->GetVariableByIndex(v);
			D3D11_SHADER_VARIABLE_DESC varDesc;
			var->GetDesc(&varDesc);

			std::wstring propName(varDesc.Name, varDesc.Name + strlen(varDesc.Name));

			// 오프셋과 크기 기록
			size_t offset = varDesc.StartOffset;
			size_t size = varDesc.Size;

			// 예: CBPropertyInfo에 저장
			CBPropertyInfo info;
			info.bufferName = cbName;
			info.offset = static_cast<UINT>(offset);
			info.size = static_cast<UINT>(size);

			m_CBPropertyMap[_type][propName] = cbName;
			// 오프셋/크기 매핑 테이블에도 저장
			m_CBPropertyOffsetMap[cbName][propName] = info;
		}
	}
}

void MMMEngine::ShaderInfo::DeSerialize()
{
	//m_SRMap[ShaderType::PBR] = 
}

void MMMEngine::ShaderInfo::StartUp()
{
	// --- JSON 템플릿 ---
	// 기본 쉐이더 정의
	m_pDefaultVShader = ResourceManager::Get().Load<VShader>(L"Shader/PBR/VS/SkeletalVertexShader.hlsl");
	m_pDefaultPShader = ResourceManager::Get().Load<PShader>(L"Shader/PBR/PS/BRDFShader.hlsl");

	// 쉐이더 타입정의
	m_shaderTypeMap[L"Shader/PBR/PS/BRDFShader.hlsl"] = ShaderType::S_PBR;

	// 렌더타입 정의
	m_renderTypeMap[L"Shader/PBR/PS/BRDFShader.hlsl"] = RenderType::R_GEOMETRY;

	// 쉐이더 리플렉션 등록 (상수버퍼 개별업데이트 사용하기 위함)
	CreateShaderReflection(L"Shader/PBR/PS/BRDFShader.hlsl");

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
	m_texPropertyMap[S_PBR][L"basecolor"] = 0;
	m_texPropertyMap[S_PBR][L"normal"] = 1;
	m_texPropertyMap[S_PBR][L"emissive"] = 2;
	m_texPropertyMap[S_PBR][L"shadowmap"] = 3;
	m_texPropertyMap[S_PBR][L"opacity"] = 4;
	m_texPropertyMap[S_PBR][L"metallic"] = 30;
	m_texPropertyMap[S_PBR][L"roughness"] = 31;
	m_texPropertyMap[S_PBR][L"ao"] = 32;

	// 상수버퍼이름 하드코딩
	m_CBPropertyMap[S_PBR][L"basecolor_factor"] = L"S_PBR_materialbuffer";
	m_CBPropertyMap[S_PBR][L"metallic_factor"] = L"S_PBR_materialbuffer";
	m_CBPropertyMap[S_PBR][L"roughness_factor"] = L"S_PBR_materialbuffer";
	m_CBPropertyMap[S_PBR][L"ao_factor"] = L"S_PBR_materialbuffer";
	m_CBPropertyMap[S_PBR][L"emissive_factor"] = L"S_PBR_materialbuffer";

	// 상수버퍼번호 하드코딩
	m_CBIndexMap[S_PBR][L"S_PBR_materialbuffer"] = 3;

	// 구조체별 이름 등록 (원래 이름과같게, 소문자로 하는것이 규칙)
	m_CBBufferMap[L"pbr_materialbuffer"] = CreateConstantBuffer<PBR_MaterialBuffer>();

	// Json 읽기
	DeSerialize();
}

void MMMEngine::ShaderInfo::ShutDown()
{
	m_shaderTypeMap.clear();
	m_typeInfo.clear();
	m_texPropertyMap.clear();
	m_CBPropertyMap.clear();
	m_CBIndexMap.clear();
	m_CBBufferMap.clear();
	m_CBPropertyOffsetMap.clear();
}

std::wstring MMMEngine::ShaderInfo::GetDefaultVShader()
{
	return m_pDefaultVShader->GetFilePath();
}

std::wstring MMMEngine::ShaderInfo::GetDefaultPShader()
{
	return m_pDefaultPShader->GetFilePath();
}

const MMMEngine::RenderType MMMEngine::ShaderInfo::GetRenderType(const std::wstring& _shaderPath)
{
	if (m_renderTypeMap.find(_shaderPath) == m_renderTypeMap.end())
		return RenderType::R_GEOMETRY;

	return m_renderTypeMap[_shaderPath];
}

const MMMEngine::ShaderType MMMEngine::ShaderInfo::GetShaderType(const std::wstring& _shaderPath)
{
	if (m_shaderTypeMap.find(_shaderPath) == m_shaderTypeMap.end())
		return ShaderType::S_PBR;

	return m_shaderTypeMap[_shaderPath];
}

// TODO :: 프로퍼티 입력받으면 업데이트하게 만들기
void MMMEngine::ShaderInfo::UpdateProperty(ID3D11DeviceContext* context,
	const ShaderType shaderType,
	const std::wstring& propertyName,
	const void* data)
{
	auto cbName = m_CBPropertyMap[shaderType][propertyName];
	auto buffer = m_CBBufferMap[cbName];
	auto& info = m_CBPropertyOffsetMap[cbName][propertyName];

	D3D11_MAPPED_SUBRESOURCE mapped;
	context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);

	memcpy((BYTE*)mapped.pData + info.offset, data, info.size);

	context->Unmap(buffer.Get(), 0);
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
		auto texIt = m_texPropertyMap.at(_type).find(_propertyName);
		if (texIt == m_texPropertyMap.at(_type).end())
			return -1;
		return texIt->second;
	}
	// 상수면 bN 반환
	else if (propIt->second == PropertyType::Constant) {
		auto cbNameIt = m_CBPropertyMap.at(_type).find(_propertyName);
		if (cbNameIt == m_CBPropertyMap.at(_type).end())
			return -1;

		auto cbIndexIt = m_CBIndexMap.at(_type).find(cbNameIt->second);
		if (cbIndexIt == m_CBIndexMap.at(_type).end())
			return -1;

		return cbIndexIt->second;
	}

	return -1;

}
