#include "ShaderInfo.h"
#include <filesystem>
#include <d3dcompiler.h>

#include "RenderManager.h"
#include "Material.h"

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

			// 타입 정보 얻기
			ID3D11ShaderReflectionType* type = var->GetType();
			D3D11_SHADER_TYPE_DESC typeDesc;
			type->GetDesc(&typeDesc);

			std::wstring propName(varDesc.Name, varDesc.Name + strlen(varDesc.Name));

			// 타입 정보 기록
			auto it = m_propertyInfoMap[_type].find(propName);
			if (it != m_propertyInfoMap[_type].end()) {
				it->second.varType = typeDesc.Type;	// D3D_SHADER_VARIABLE_TYPE
				it->second.rows = typeDesc.Rows;
				it->second.columns = typeDesc.Columns;
			}

			// 오프셋과 크기 기록
			CBPropertyInfo cbinfo;
            cbinfo.bufferName = cbName;
            cbinfo.offset     = varDesc.StartOffset;
            cbinfo.size       = varDesc.Size;

			// 오프셋/크기 매핑 테이블에도 저장
			m_CBPropertyMap[_type][propName] = cbinfo;
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
	// 타입별 레지스터 번호 등록
	m_propertyInfoMap[ShaderType::S_PBR][L"_albedo"] = { PropertyType::Texture, 0 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_normal"] = { PropertyType::Texture, 1 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_emissive"] = { PropertyType::Texture, 2 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_shadowmap"] = { PropertyType::Texture, 3 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_opacity"] = { PropertyType::Texture, 4 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_specular"] = { PropertyType::Texture, 20 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_irradiance"] = { PropertyType::Texture, 21 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_brdflut"] = { PropertyType::Texture, 22 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_metallic"] = { PropertyType::Texture, 30 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_roughness"] = { PropertyType::Texture, 31 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_ambientOcclusion"] = { PropertyType::Texture, 32 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_sp0"] = { PropertyType::Sampler, 0 };

	m_propertyInfoMap[ShaderType::S_PBR][L"mLightDir"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mLightPadding"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mLightColor"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mIntensity"] = { PropertyType::Constant, 1 };

	m_propertyInfoMap[ShaderType::S_PBR][L"mBaseColor"] = { PropertyType::Constant, 3 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mMetallic"] = { PropertyType::Constant, 3 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mRoughness"] = { PropertyType::Constant, 3 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mAoStrength"] = { PropertyType::Constant, 3 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mEmissive"] = { PropertyType::Constant, 3 };

	// 쉐이더 타입정보정의
	m_typeInfoMap[L"Shader/PBR/PS/BRDFShader.hlsl"] = { ShaderType::S_PBR, RenderType::R_GEOMETRY, nullptr };

	// 구조체별 이름 등록 (쉐이더 이름과같게)
	m_CBBufferMap[L"MatBuffer"] = CreateConstantBuffer<PBR_MaterialBuffer>();
	m_CBBufferMap[L"LightBuffer"] = CreateConstantBuffer<Render_LightBuffer>();

	// 사용 상수버퍼 등록
	m_typeBufferMap[ShaderType::S_PBR].push_back({ L"MatBuffer" , 3 });
	m_typeBufferMap[ShaderType::S_PBR].push_back({ L"LightBuffer" , 1 });

	// 쉐이더 리플렉션 등록 (상수버퍼 개별업데이트 사용하기 위함)
	CreatePShaderReflection(L"Shader/PBR/PS/BRDFShader.hlsl");
	CreatePShaderReflection(L"Shader/SkyBox/SkyBoxPixelShader.hlsl");

	// 기본 쉐이더 정의
	m_pDefaultVShader = ResourceManager::Get().Load<VShader>(L"Shader/PBR/VS/SkeletalVertexShader.hlsl");
	m_pDefaultPShader = ResourceManager::Get().Load<PShader>(L"Shader/PBR/PS/BRDFShader.hlsl");
	// --- JSON 템플릿 ---

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
		if (SUCCEEDED(context->Map(buffer.Get(), 0, D3D11_MAP_WRITE_NO_OVERWRITE, 0, &mapped)))
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

void MMMEngine::ShaderInfo::UpdateCBuffers(const ShaderType _type)
{
	auto it = m_typeBufferMap.find(_type);
	if (it == m_typeBufferMap.end())
		return;

	for (auto& bufferInfo : it->second) {
		auto cbit = m_CBBufferMap.find(bufferInfo.bufferName);

		if (cbit != m_CBBufferMap.end()) {
			RenderManager::Get().GetContext()->PSSetConstantBuffers(bufferInfo.registerIndex, 1,
				cbit->second.GetAddressOf());
		}
	}
}

void AddProperty(MMMEngine::Material* _mat, const std::wstring& propName, const MMMEngine::PropertyInfo& pInfo)
{
	MMMEngine::PropertyValue val;

	switch (pInfo.propertyType) // category: Constant / Texture / Sampler
	{
	case MMMEngine::PropertyType::Constant:
		// ConstantBuffer 변수 타입에 따라 기본값 설정
		switch (pInfo.varType) // D3D_SHADER_VARIABLE_TYPE
		{
		case D3D_SVT_FLOAT:
			if (pInfo.columns == 1 && pInfo.rows == 1) {
				val = 0.0f;
			}
			else if (pInfo.columns == 3 && pInfo.rows == 1) {
				val = DirectX::SimpleMath::Vector3::Zero;
			}
			else if (pInfo.columns == 4 && pInfo.rows == 1) {
				val = DirectX::SimpleMath::Vector4::Zero;
			}
			else if (pInfo.columns == 4 && pInfo.rows == 4) {
				val = DirectX::SimpleMath::Matrix::Identity;
			}
			break;

		case D3D_SVT_INT:
			val = 0;
			break;

		case D3D_SVT_BOOL:
			val = false;
			break;

		default:
			// 기타 타입은 기본값 없음
			val = 0;
			break;
		}
		break;

	case MMMEngine::PropertyType::Texture:
	{
		MMMEngine::ResPtr<MMMEngine::Texture2D> tex;
		if(propName == L"_normal")
			tex = MMMEngine::ResourceManager::Get()
			.Load<MMMEngine::Texture2D>(L"Shader/Resource/Default_Texture/Default_Normal.png");
		else
			tex = MMMEngine::ResourceManager::Get()
			.Load<MMMEngine::Texture2D>(L"Shader/Resource/Default_Texture/Solid_White.png");
		val = tex; // 기본 텍스처
	}
	break;

	case MMMEngine::PropertyType::Sampler:
	{
		Microsoft::WRL::ComPtr<ID3D11SamplerState> sampler = nullptr;
		val = nullptr; // 기본 샘플러 없음 // TODO::ShaderInfo 기본 샘플러 만들기
	}
	break;
	}

	// Material에 프로퍼티 등록
	_mat->AddProperty(propName, val);
}

void MMMEngine::ShaderInfo::ConvertMaterialType(const ShaderType _type, Material* _mat)
{
	// ShaderType에 등록된 프로퍼티 맵 조회
	auto propIt = m_propertyInfoMap.find(_type);
	if (propIt == m_propertyInfoMap.end())
		return;

	const auto& shaderProps = propIt->second;

	// 1. ShaderType에 있는 모든 프로퍼티를 Material에 반영
	for (const auto& [propName, pinfo] : shaderProps)
	{
		// TODO :: 기본Sampler 만들고나서 이거 지우기
		if (pinfo.propertyType == PropertyType::Sampler)
			continue;

		// Material에 해당 프로퍼티가 없으면 추가
		if (_mat->m_properties.find(propName) == _mat->m_properties.end())
		{
			AddProperty(_mat, propName, pinfo);
		}
	}

	// 2. Material에 있는 프로퍼티 중 ShaderType에 없는 것 제거
	std::vector<std::wstring> toRemove;
	for (const auto& [name, val] : _mat->m_properties)
	{
		if (shaderProps.find(name) == shaderProps.end())
		{
			toRemove.push_back(name);
		}
	}

	for (const auto& propName : toRemove)
	{
		_mat->RemoveProperty(propName);
	}
}
