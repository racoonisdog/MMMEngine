#include "ShaderInfo.h"
#include <filesystem>
#include <d3dcompiler.h>

#include "RenderManager.h"

namespace fs = std::filesystem;

DEFINE_SINGLETON(MMMEngine::ShaderInfo);

void MMMEngine::ShaderInfo::CreatePShaderReflection(std::wstring&& _filePath)
{
	// 타입 검색
	auto typeIt = m_typeInfoMap.find(_filePath);
	if (typeIt == m_typeInfoMap.end())
		throw std::runtime_error("ShaderInfo::CreateShaderReflection : Shader Type not found !!");

	ShaderType _type = typeIt->second.shaderType;

	// 절대경로 만들기
	fs::path realPath(ResourceManager::Get().GetCurrentRootPath());
	realPath = realPath / _filePath;

	fs::path filePath (realPath);
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

	// SRV 인덱스 등록
	for (UINT r = 0; r < shaderDesc.BoundResources; r++) {
		D3D11_SHADER_INPUT_BIND_DESC bindDesc;
		reflection->GetResourceBindingDesc(r, &bindDesc);

		std::wstring propName(bindDesc.Name, bindDesc.Name + strlen(bindDesc.Name));

		PropertyInfo pinfo;

		if (bindDesc.Type == D3D_SIT_TEXTURE)
		{
			pinfo.propertyType = PropertyType::Texture;
			pinfo.bufferIndex = bindDesc.BindPoint; // tN 슬롯
			m_propertyInfoMap[_type][propName] = pinfo;
		}
		else if (bindDesc.Type == D3D_SIT_SAMPLER)
		{
			pinfo.propertyType = PropertyType::Sampler;
			pinfo.bufferIndex = bindDesc.BindPoint; // sN 슬롯
			m_propertyInfoMap[_type][propName] = pinfo;
		}
	}

	// ConstantBuffer 등록
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
			CBPropertyInfo cbinfo;
            cbinfo.bufferName = cbName;
            cbinfo.offset     = varDesc.StartOffset;
            cbinfo.size       = varDesc.Size;

			// 오프셋/크기 매핑 테이블에도 저장
			m_CBPropertyMap[_type][propName] = cbinfo;

			// 프로퍼티 타입 등록
			PropertyInfo pinfo;
			pinfo.propertyType = PropertyType::Constant;
			pinfo.bufferIndex = i; // bN 슬롯 번호 (BindPoint로 바꿔도 가능)
			m_propertyInfoMap[_type][propName] = pinfo;
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

	// 쉐이더 타입정보정의
	m_typeInfoMap[L"Shader/PBR/PS/BRDFShader.hlsl"] = { ShaderType::S_PBR, RenderType::R_GEOMETRY, nullptr };

	// 쉐이더 리플렉션 등록 (상수버퍼 개별업데이트 사용하기 위함)
	CreatePShaderReflection(L"Shader/PBR/PS/BRDFShader.hlsl");

	// 구조체별 이름 등록 (원래 이름과같게, 소문자로 하는것이 규칙)
	m_CBBufferMap[L"MatBuffer"] = CreateConstantBuffer<PBR_MaterialBuffer>();

	// Json 읽기
	DeSerialize();
}

void MMMEngine::ShaderInfo::ShutDown()
{
	m_pDefaultPShader.reset();
	m_pDefaultPShader.reset();

	m_typeInfoMap.clear();
	m_propertyInfoMap.clear();
	m_CBPropertyMap.clear();
	m_CBBufferMap.clear();
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
	if (m_typeInfoMap.find(_shaderPath) == m_typeInfoMap.end())
		return RenderType::R_GEOMETRY;

	return m_typeInfoMap[_shaderPath].renderType;
}

const MMMEngine::ShaderType MMMEngine::ShaderInfo::GetShaderType(const std::wstring& _shaderPath)
{
	if (m_typeInfoMap.find(_shaderPath) == m_typeInfoMap.end())
		return ShaderType::S_PBR;

	return m_typeInfoMap[_shaderPath].shaderType;
}

void MMMEngine::ShaderInfo::UpdateProperty(ID3D11DeviceContext4* context,
	const ShaderType shaderType,
	const std::wstring& propertyName,
	const void* data)
{
	// PropertyInfo 조회
	auto propIt = m_propertyInfoMap.find(shaderType);
	if (propIt == m_propertyInfoMap.end())
		return;

	auto& propMap = propIt->second;
	auto pinfoIt = propMap.find(propertyName);
	if (pinfoIt == propMap.end())
		return;

	const PropertyInfo& pinfo = pinfoIt->second;

	if (pinfo.propertyType == PropertyType::Constant) {
		// ConstantBuffer 변수 정보 조회
		auto cbIt = m_CBPropertyMap[shaderType].find(propertyName);
		if (cbIt == m_CBPropertyMap[shaderType].end())
			return;

		const CBPropertyInfo& cbInfo = cbIt->second;

		auto bufIt = m_CBBufferMap.find(cbInfo.bufferName);
		if (bufIt == m_CBBufferMap.end())
			return;

		auto buffer = bufIt->second;

		D3D11_MAPPED_SUBRESOURCE mapped;
		if (SUCCEEDED(context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy((BYTE*)mapped.pData + cbInfo.offset, data, cbInfo.size);
			context->Unmap(buffer.Get(), 0);
		}
	}
	else if (pinfo.propertyType == PropertyType::Texture) {
		// Texture는 SRV 바인딩만 하면 됨
		ID3D11ShaderResourceView* srv = reinterpret_cast<ID3D11ShaderResourceView*>(const_cast<void*>(data));
		context->PSSetShaderResources(pinfo.bufferIndex, 1, &srv);
	}
	else if (pinfo.propertyType == PropertyType::Sampler) {
		// SamplerState는 Sampler 슬롯에 바인딩
		ID3D11SamplerState* sampler = reinterpret_cast<ID3D11SamplerState*>(const_cast<void*>(data));
		context->PSSetSamplers(pinfo.bufferIndex, 1, &sampler);
	}
}

const int MMMEngine::ShaderInfo::PropertyToIdx(const ShaderType _type, const std::wstring& _propertyName, PropertyInfo* _out /*= nullptr*/) const
{
	// ShaderType 존재 확인
	auto typeIt = m_propertyInfoMap.find(_type);
	if (typeIt == m_propertyInfoMap.end())
		return -1;

	// PropertyName 존재 확인
	auto propIt = typeIt->second.find(_propertyName);
	if (propIt == typeIt->second.end())
		return -1;

	// 타입 반환
	if (_out)
		*_out = propIt->second;

	// bufferIndex 반환 (Texture면 tN, Constant면 bN 슬롯 번호)
	return propIt->second.bufferIndex;

}
