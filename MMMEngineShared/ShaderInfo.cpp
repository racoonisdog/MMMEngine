#include "ShaderInfo.h"
#include <filesystem>
#include <d3dcompiler.h>

#include "RenderManager.h"
#include "Material.h"

namespace fs = std::filesystem;

DEFINE_SINGLETON(MMMEngine::ShaderInfo);

void MMMEngine::ShaderInfo::EnsureCBStaging(const std::wstring& cbName, UINT cbSize)
{
	auto& buf = m_CBStaging[cbName];
	if (buf.size() != cbSize)
		buf.assign(cbSize, 0); // 0으로 초기화
}

void MMMEngine::ShaderInfo::UploadCB(ID3D11DeviceContext4* context, const std::wstring& cbName)
{
	auto bIt = m_CBBufferMap.find(cbName);
	if (bIt == m_CBBufferMap.end())
		return;

	auto sIt = m_CBStaging.find(cbName);
	if (sIt == m_CBStaging.end())
		return;

	auto& staging = sIt->second;
	if (staging.empty())
		return;

	D3D11_MAPPED_SUBRESOURCE mapped{};
	if (SUCCEEDED(context->Map(bIt->second.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
	{
		memcpy(mapped.pData, staging.data(), staging.size());
		context->Unmap(bIt->second.Get(), 0);
	}
}

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

	if (!res) {
		std::cerr << "ShaderInfo::CreateShaderReflection : Shader Load Fail !!" << std::endl;
		return;
	}

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

		// CPU 버퍼에 사이즈 저장
		m_CBSizeMap[cbName] = cbDesc.Size;
		EnsureCBStaging(cbName, cbDesc.Size);

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
	// 쉐이더 타입정보정의
	m_typeInfoMap[L"Shader/PBR/PS/BRDFShader.hlsl"] = { ShaderType::S_PBR, RenderType::R_GEOMETRY };
	m_typeInfoMap[L"Shader/PBR/PS/TrailUnlitPS.hlsl"] = { ShaderType::S_PBR, RenderType::R_GEOMETRY };
	m_typeInfoMap[L"Shader/PBR/PS/LineUnlitPS.hlsl"] = { ShaderType::S_PBR, RenderType::R_PARTICLE };
	m_typeInfoMap[L"Shader/TOON/ToonPS.hlsl"] = { ShaderType::S_TOON, RenderType::R_GEOMETRY };
	m_typeInfoMap[L"Shader/Snow/TestSnowPS.hlsl"] = { ShaderType::S_SNOW, RenderType::R_GEOMETRY };
	m_typeInfoMap[L"Shader/SkyBox/SkyBoxPixelShader.hlsl"] = { ShaderType::S_SKYBOX, RenderType::R_SKYBOX };


	// 구조체별 이름 등록 (쉐이더 이름과같게)
	m_CBBufferMap[L"MatBuffer"] = CreateConstantBuffer<PBR_MaterialBuffer>();
	m_CBBufferMap[L"LightBuffer"] = CreateConstantBuffer<Render_LightBuffer>();
	m_CBBufferMap[L"ToonMatBuffer"] = CreateConstantBuffer<TOON_MaterialBuffer>();
	m_CBBufferMap[L"SnowParams"] = CreateConstantBuffer<SNOW_SnowParams>();
	

	// 사용 상수버퍼 등록
	m_typeBufferMap[ShaderType::S_PBR].push_back({ L"MatBuffer" , 3 });
	m_typeBufferMap[ShaderType::S_PBR].push_back({ L"LightBuffer" , 1 });

	m_typeBufferMap[ShaderType::S_TOON].push_back({ L"LightBuffer" , 1 });
	m_typeBufferMap[ShaderType::S_TOON].push_back({ L"ToonMatBuffer" , 3 });

	m_typeBufferMap[ShaderType::S_SNOW].push_back({ L"SnowParams" , 5 });

	// 타입별 레지스터 번호 등록
	m_propertyInfoMap[ShaderType::S_PBR][L"_albedo"] = { PropertyType::Texture, 0 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_normal"] = { PropertyType::Texture, 1, ResourceManager::Get().Load<Texture2D>(L"Shader/Resource/Default_Texture/Default_Normal.png") };
	m_propertyInfoMap[ShaderType::S_PBR][L"_emissive"] = { PropertyType::Texture, 2 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_shadowmap"] = { PropertyType::Texture, 3 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_opacity"] = { PropertyType::Texture, 4 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_specular"] = { PropertyType::Texture, 20 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_irradiance"] = { PropertyType::Texture, 21 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_brdflut"] = { PropertyType::Texture, 22 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_metallic"] = { PropertyType::Texture, 30 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_roughness"] = { PropertyType::Texture, 31 };
	m_propertyInfoMap[ShaderType::S_PBR][L"_ambientOcclusion"] = { PropertyType::Texture, 32 };
	//m_propertyInfoMap[ShaderType::S_PBR][L"_sp0"] = { PropertyType::Sampler, 0 };

	m_propertyInfoMap[ShaderType::S_PBR][L"mLightDir"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mLightPadding"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mLightColor"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mIntensity"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mLightPos"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mBaseColor"] = { PropertyType::Constant, 3, DirectX::SimpleMath::Vector4{1.0f, 1.0f, 1.0f, 1.0f} };
	m_propertyInfoMap[ShaderType::S_PBR][L"mMetallic"] = { PropertyType::Constant, 3 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mRoughness"] = { PropertyType::Constant, 3 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mAoStrength"] = { PropertyType::Constant, 3, 0.5f };
	m_propertyInfoMap[ShaderType::S_PBR][L"mEmissive"] = { PropertyType::Constant, 3 };
	m_propertyInfoMap[ShaderType::S_PBR][L"mRoundDotClip"] = { PropertyType::Constant, 3, 0.0f };

	//
	m_propertyInfoMap[ShaderType::S_TOON][L"_albedo"] = { PropertyType::Texture, 0 };
	m_propertyInfoMap[ShaderType::S_TOON][L"_normal"] = { PropertyType::Texture, 1, ResourceManager::Get().Load<Texture2D>(L"Shader/Resource/Default_Texture/Default_Normal.png") };
	m_propertyInfoMap[ShaderType::S_TOON][L"_emissive"] = { PropertyType::Texture, 2 };
	m_propertyInfoMap[ShaderType::S_TOON][L"_shadowmap"] = { PropertyType::Texture, 3 };
	m_propertyInfoMap[ShaderType::S_TOON][L"_opacity"] = { PropertyType::Texture, 4 };
	m_propertyInfoMap[ShaderType::S_TOON][L"_lutMap"] = { PropertyType::Texture, 10, ResourceManager::Get().Load<Texture2D>(L"Shader/Resource/Default_Texture/Toon_Lut.png") };
	m_propertyInfoMap[ShaderType::S_TOON][L"_roughness"] = { PropertyType::Texture, 11 };
	m_propertyInfoMap[ShaderType::S_TOON][L"_ambientOcclusion"] = { PropertyType::Texture, 12 };
	m_propertyInfoMap[ShaderType::S_TOON][L"_specular"] = { PropertyType::Texture, 20 };
	m_propertyInfoMap[ShaderType::S_TOON][L"_irradiance"] = { PropertyType::Texture, 21 };
	m_propertyInfoMap[ShaderType::S_TOON][L"_brdflut"] = { PropertyType::Texture, 22 };

	m_propertyInfoMap[ShaderType::S_TOON][L"mLightDir"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_TOON][L"mLightPadding"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_TOON][L"mLightColor"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_TOON][L"mIntensity"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_TOON][L"mLightPos"] = { PropertyType::Constant, 1 };
	m_propertyInfoMap[ShaderType::S_TOON][L"mBaseColor"] = { PropertyType::Constant, 3, DirectX::SimpleMath::Vector4{1.0f, 1.0f, 1.0f, 1.0f} };
	m_propertyInfoMap[ShaderType::S_TOON][L"mAoStrength"] = { PropertyType::Constant, 3, 0.5f };
	m_propertyInfoMap[ShaderType::S_TOON][L"mDiffuseStr"] = { PropertyType::Constant, 3, 0.5f };
	m_propertyInfoMap[ShaderType::S_TOON][L"mSpecularStr"] = { PropertyType::Constant, 3, 0.1f };
	m_propertyInfoMap[ShaderType::S_TOON][L"mRoughness"] = { PropertyType::Constant, 3 };
	m_propertyInfoMap[ShaderType::S_TOON][L"mLowLut"] = { PropertyType::Constant, 3, 0.3f };
	m_propertyInfoMap[ShaderType::S_TOON][L"mDiffGradientDistHalf"] = { PropertyType::Constant, 3, 0.15625f };
	m_propertyInfoMap[ShaderType::S_TOON][L"mDiffGradientDepth"] = { PropertyType::Constant, 3, 68.3263f };
	m_propertyInfoMap[ShaderType::S_TOON][L"mRimLightStr"] = { PropertyType::Constant, 3, 0.1f };
	m_propertyInfoMap[ShaderType::S_TOON][L"mEmissive"] = { PropertyType::Constant, 3 };
	m_propertyInfoMap[ShaderType::S_TOON][L"mPadding"] = { PropertyType::Constant, 3 };
	
	//
	m_propertyInfoMap[ShaderType::S_SKYBOX][L"_cubemap"]	= { PropertyType::Texture, 0 };
	//m_propertyInfoMap[ShaderType::S_SKYBOX][L"_sp0"]		= { PropertyType::Sampler, 0 };

	// --- Snow ---
	m_propertyInfoMap[ShaderType::S_SNOW][L"_albedo"] = { PropertyType::Texture, 0 };
	m_propertyInfoMap[ShaderType::S_SNOW][L"_normal"] = { PropertyType::Texture, 1, ResourceManager::Get().Load<Texture2D>(L"Shader/Resource/Default_Texture/Default_Normal.png") };
	m_propertyInfoMap[ShaderType::S_SNOW][L"_emissive"] = { PropertyType::Texture, 2 };
	m_propertyInfoMap[ShaderType::S_SNOW][L"_shadowmap"] = { PropertyType::Texture, 3 };
	m_propertyInfoMap[ShaderType::S_SNOW][L"_opacity"] = { PropertyType::Texture, 4 };
	m_propertyInfoMap[ShaderType::S_SNOW][L"_ambientOcclusion"] = { PropertyType::Texture, 10 };
	m_propertyInfoMap[ShaderType::S_SNOW][L"_lutMap"] = { PropertyType::Texture, 11, ResourceManager::Get().Load<Texture2D>(L"Shader/Resource/Default_Texture/Toon_Lut.png") };

	// SnowParams(b5)
	m_propertyInfoMap[ShaderType::S_SNOW][L"tileScale"] = { PropertyType::Constant, 5, 0.15f };
	m_propertyInfoMap[ShaderType::S_SNOW][L"warpStrength"] = { PropertyType::Constant, 5, 0.25f };
	m_propertyInfoMap[ShaderType::S_SNOW][L"windStrength"] = { PropertyType::Constant, 5, 0.2f };
	m_propertyInfoMap[ShaderType::S_SNOW][L"iceStrength"] = { PropertyType::Constant, 5, 0.4f };

	m_propertyInfoMap[ShaderType::S_SNOW][L"octaves"] = { PropertyType::Constant, 5, 4 };
	m_propertyInfoMap[ShaderType::S_SNOW][L"mAoStrength"] = { PropertyType::Constant, 5, 0.2f };
	m_propertyInfoMap[ShaderType::S_SNOW][L"padding"] = { PropertyType::Constant, 5 };
	//---

	// 쉐이더 리플렉션 등록 (상수버퍼 개별업데이트 사용하기 위함) (!! 순서중요)
	CreatePShaderReflection(L"Shader/PBR/PS/BRDFShader.hlsl");
	CreatePShaderReflection(L"Shader/SkyBox/SkyBoxPixelShader.hlsl");
	CreatePShaderReflection(L"Shader/TOON/ToonPS.hlsl");
	CreatePShaderReflection(L"Shader/Snow/TestSnowPS.hlsl");

	// 기본 쉐이더 정의
	m_pDefaultVShader = ResourceManager::Get().Load<VShader>(L"Shader/PBR/VS/StaticVertexShader.hlsl");
	m_pSkeletalVShader = ResourceManager::Get().Load<VShader>(L"Shader/PBR/VS/SkeletalVertexShader.hlsl");
	m_pDefaultPShader = ResourceManager::Get().Load<PShader>(L"Shader/PBR/PS/BRDFShader.hlsl");

	m_pFullScreenVS = ResourceManager::Get().Load<VShader>(L"Shader/PP/FullScreenVS.hlsl");
	m_pFullScreenPS = ResourceManager::Get().Load<PShader>(L"Shader/PP/FullScreenPS.hlsl");

	m_pShadowPS = ResourceManager::Get().Load<PShader>(L"Shader/Shadow/ShadowPS.hlsl");
	// --- JSON 템플릿 ---
	
	// Json 읽기
	DeSerialize();
}

void MMMEngine::ShaderInfo::ShutDown()
{
	m_pDefaultVShader.reset();
	m_pSkeletalVShader.reset();
	m_pDefaultPShader.reset();
	m_pFullScreenVS.reset();
	m_pFullScreenPS.reset();
	m_pShadowPS.reset();

	m_typeInfoMap.clear();
	m_propertyInfoMap.clear();
	m_CBPropertyMap.clear();
	m_CBBufferMap.clear();
	m_CBStaging.clear();
	m_CBSizeMap.clear();
	m_globalPropMap.clear();
	m_typeBufferMap.clear();
}

MMMEngine::ResPtr<MMMEngine::VShader> MMMEngine::ShaderInfo::GetDefaultVShader()
{
	return m_pDefaultVShader;
}

MMMEngine::ResPtr<MMMEngine::VShader> MMMEngine::ShaderInfo::GetSkeletalVShader()
{
	return m_pSkeletalVShader;
}

MMMEngine::ResPtr<MMMEngine::PShader> MMMEngine::ShaderInfo::GetDefaultPShader()
{
	return m_pDefaultPShader;
}

MMMEngine::ResPtr<MMMEngine::VShader> MMMEngine::ShaderInfo::GetFullScreenVShader()
{
	return m_pFullScreenVS;
}

MMMEngine::ResPtr<MMMEngine::PShader> MMMEngine::ShaderInfo::GetFullScreenPShader()
{
	return m_pFullScreenPS;
}

MMMEngine::ResPtr<MMMEngine::PShader> MMMEngine::ShaderInfo::GetShadowPShader()
{
	return m_pShadowPS;
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
	// 1. 글로벌 프로퍼티 먼저 확인
	auto gIt = m_globalPropMap.find(shaderType);
	if (gIt != m_globalPropMap.end())
	{
		auto& gPropMap = gIt->second;
		auto gPropIt = gPropMap.find(propertyName);
		if (gPropIt != gPropMap.end())
		{
			const PropertyValue& gval = gPropIt->second;
			const PropertyInfo& pinfo = m_propertyInfoMap[shaderType][propertyName];

			if (pinfo.propertyType == PropertyType::Constant)
			{
				// 글로벌 ConstantBuffer 값 적용
				auto cbIt = m_CBPropertyMap[shaderType].find(propertyName);
				if (cbIt == m_CBPropertyMap[shaderType].end())
					return;

				const CBPropertyInfo& cbInfo = cbIt->second;
				auto bufIt = m_CBBufferMap.find(cbInfo.bufferName);
				if (bufIt == m_CBBufferMap.end())
					return;

				auto buffer = bufIt->second;

				// staging 확보
				auto szIt = m_CBSizeMap.find(cbInfo.bufferName);
				if (szIt == m_CBSizeMap.end()) return;
				EnsureCBStaging(cbInfo.bufferName, szIt->second);

				auto& staging = m_CBStaging[cbInfo.bufferName];

				// 해당 영역을 0으로 한번 밀고(패딩/정렬 이슈 대비)
				if (cbInfo.offset + cbInfo.size <= staging.size())
				{
					memset(staging.data() + cbInfo.offset, 0, cbInfo.size);

					std::visit([&](auto&& arg) {
						const size_t copySize = std::min<size_t>(sizeof(arg), cbInfo.size);
						memcpy(staging.data() + cbInfo.offset, &arg, copySize);
						}, gval);

					// CB 전체 업로드 (WRITE_DISCARD)
					UploadCB(context, cbInfo.bufferName);
				}
				return;
			}
			else if (pinfo.propertyType == PropertyType::Texture)
			{
				auto texPtr = std::get<ResPtr<Texture2D>>(gval);
				if (!texPtr)
					return;

				ID3D11ShaderResourceView* srv =
					texPtr->m_pSRV.Get();
				context->PSSetShaderResources(pinfo.bufferIndex, 1, &srv);
				return;
			}
			else if (pinfo.propertyType == PropertyType::Sampler)
			{
				return;
			}
		}
	}

	// 2. 글로벌에 없으면 머티리얼 프로퍼티 업데이트 (기존 로직)
	auto propIt = m_propertyInfoMap.find(shaderType);
	if (propIt == m_propertyInfoMap.end())
		return;

	auto& propMap = propIt->second;
	auto pinfoIt = propMap.find(propertyName);
	if (pinfoIt == propMap.end())
		return;

	const PropertyInfo& pinfo = pinfoIt->second;

	if (pinfo.propertyType == PropertyType::Constant)
	{
		auto cbIt = m_CBPropertyMap[shaderType].find(propertyName);
		if (cbIt == m_CBPropertyMap[shaderType].end())
			return;

		const CBPropertyInfo& cbInfo = cbIt->second;
		auto bufIt = m_CBBufferMap.find(cbInfo.bufferName);
		if (bufIt == m_CBBufferMap.end())
			return;

		auto buffer = bufIt->second;

		auto szIt = m_CBSizeMap.find(cbInfo.bufferName);
		if (szIt == m_CBSizeMap.end()) return;
		EnsureCBStaging(cbInfo.bufferName, szIt->second);

		auto& staging = m_CBStaging[cbInfo.bufferName];

		if (cbInfo.offset + cbInfo.size <= staging.size())
		{
			// 해당 영역을 0으로 밀고 복사 (패딩 대비)
			memset(staging.data() + cbInfo.offset, 0, cbInfo.size);
			memcpy(staging.data() + cbInfo.offset, data, cbInfo.size);

			UploadCB(context, cbInfo.bufferName);
		}
	}
	else if (pinfo.propertyType == PropertyType::Texture)
	{
		ID3D11ShaderResourceView* srv =
			reinterpret_cast<ID3D11ShaderResourceView*>(const_cast<void*>(data));
		context->PSSetShaderResources(pinfo.bufferIndex, 1, &srv);
	}
	else if (pinfo.propertyType == PropertyType::Sampler)
	{
		return;
		/*ID3D11SamplerState* sampler =
			reinterpret_cast<ID3D11SamplerState*>(const_cast<void*>(data));
		context->PSSetSamplers(pinfo.bufferIndex, 1, &sampler);*/
	}

}

const int MMMEngine::ShaderInfo::PropertyToIdx(const ShaderType _type, const std::wstring& _propertyName, PropertyInfo* _out /*= nullptr*/) const
{
	// ShaderType 존재 확인 (글로벌 먼저 확인)
	auto typeIt = m_propertyInfoMap.find(_type);
	if (typeIt == m_propertyInfoMap.end())
		return -1;

	// PropertyName 존재 확인 (글로벌 먼저 확인)
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
				auto getted = std::get_if<float>(&pInfo.defaultValue);
				if (getted == nullptr)
					val = 0.0f;
				else
					val = *getted;
			}
			else if (pInfo.columns == 3 && pInfo.rows == 1) {
				auto getted = std::get_if<DirectX::SimpleMath::Vector3>(&pInfo.defaultValue);
				if (getted == nullptr)
					val = DirectX::SimpleMath::Vector3::Zero;
				else
					val = *getted;
			}
			else if (pInfo.columns == 4 && pInfo.rows == 1) {
				auto getted = std::get_if<DirectX::SimpleMath::Vector4>(&pInfo.defaultValue);
				if (getted == nullptr)
					val = DirectX::SimpleMath::Vector4::Zero;
				else
					val = *getted;
			}
			else if (pInfo.columns == 4 && pInfo.rows == 4) {
				auto getted = std::get_if<DirectX::SimpleMath::Matrix>(&pInfo.defaultValue);
				if (getted == nullptr)
					val = DirectX::SimpleMath::Matrix::Identity;
				else
					val = *getted;
			}
			break;

		case D3D_SVT_INT:
		{
			auto getted = std::get_if<int>(&pInfo.defaultValue);
			if (getted == nullptr)
				val = 0;
			else
				val = *getted;
			break;
		}

		case D3D_SVT_BOOL:
		{
			auto getted = std::get_if<int>(&pInfo.defaultValue);
			if (getted == nullptr)
				val = false;
			else
				val = *getted;
			break;
		}

		default:
			// 기타 타입은 기본값 없음
			val = 0;
			break;
		}
		break;

	case MMMEngine::PropertyType::Texture:
	{
		MMMEngine::ResPtr<MMMEngine::Texture2D> tex;

		auto texture = std::get_if<MMMEngine::ResPtr<MMMEngine::Texture2D>>(&pInfo.defaultValue);

		if (texture == nullptr) {
			tex = MMMEngine::ResourceManager::Get()
				.Load<MMMEngine::Texture2D>(L"Shader/Resource/Default_Texture/Solid_White.png");
		}
		else {
			tex = *texture;
		}
		/*if(propName == L"_normal")
			tex = MMMEngine::ResourceManager::Get()
			.Load<MMMEngine::Texture2D>(L"Shader/Resource/Default_Texture/Default_Normal.png");
		else
			tex = MMMEngine::ResourceManager::Get()
			.Load<MMMEngine::Texture2D>(L"Shader/Resource/Default_Texture/Solid_White.png");*/
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

const MMMEngine::PropertyValue& MMMEngine::ShaderInfo::GetGlobalPropVal(const ShaderType _type, const std::wstring _propName)
{
	static const MMMEngine::PropertyValue emptyValue{}; // 기본 생성된 빈 값

	// 글로벌 맵에 있는지 확인
	auto tit = m_globalPropMap.find(_type);
	if (tit == m_globalPropMap.end())
		return emptyValue;
	auto nit = tit->second.find(_propName);
	if (nit == tit->second.end())
		return emptyValue;

	return nit->second;
}

void MMMEngine::ShaderInfo::AddGlobalPropVal(const ShaderType _type, const std::wstring _propName, const PropertyValue& _value)
{
	// 인포맵에 있는지 확인
	auto tit = m_propertyInfoMap.find(_type);
	if (tit == m_propertyInfoMap.end())
		return;
	auto nit = tit->second.find(_propName);
	if (nit == tit->second.end())
		return;

	m_globalPropMap[_type][_propName] = _value;
}

void MMMEngine::ShaderInfo::AddAllGlobalPropVal(const std::wstring _propName, const PropertyValue& _value)
{
	// 인포맵에 있는지 확인
	for (auto& [key, map] : m_propertyInfoMap) {
		auto it = map.find(_propName);
		if(it == map.end())
			continue;

		m_globalPropMap[key][_propName] = _value;
	}
}

void MMMEngine::ShaderInfo::SetGlobalPropVal(const ShaderType _type, const std::wstring _propName, const PropertyValue& _value)
{
	auto tit = m_globalPropMap.find(_type);
	if (tit == m_globalPropMap.end())
		return;
	
	auto nit = tit->second.find(_propName);
	if (nit == tit->second.end())
		return;
	
	if (nit->second.index() == _value.index()) {
		nit->second = _value;
	}
}

void MMMEngine::ShaderInfo::SetAllGlobalPropVal(const std::wstring _propName, const PropertyValue& _value)
{
	for (auto& [key, map] : m_globalPropMap) {
		auto it = map.find(_propName);
		if (it == map.end())
			continue;

		if (it->second.index() == _value.index()) {
			it->second = _value;
		}
	}
}

void MMMEngine::ShaderInfo::RemoveGlobalPropVal(const ShaderType _type, const std::wstring _propName)
{
	auto tit = m_globalPropMap.find(_type);
	if (tit == m_globalPropMap.end())
		return;

	auto nit = tit->second.find(_propName);
	if (nit == tit->second.end())
		return;

	tit->second.erase(nit);
}

void MMMEngine::ShaderInfo::RemoveAllGlobalPropVal(const std::wstring _propName)
{
	for (auto& [key, map] : m_globalPropMap) {
		auto it = map.find(_propName);
		if (it == map.end())
			continue;

		map.erase(it);
	}
}

Microsoft::WRL::ComPtr<ID3D11InputLayout> MMMEngine::ShaderInfo::CreateVShaderLayout(ID3D10Blob* _blob)
{
	Microsoft::WRL::ComPtr<ID3D11ShaderReflection> reflector;
	D3DReflect(_blob->GetBufferPointer(),
		_blob->GetBufferSize(),
		IID_PPV_ARGS(&reflector));

	D3D11_SHADER_DESC shaderDesc;
	reflector->GetDesc(&shaderDesc);

	std::vector<D3D11_INPUT_ELEMENT_DESC> inputLayoutDesc;

	for (UINT i = 0; i < shaderDesc.InputParameters; i++)
	{
		D3D11_SIGNATURE_PARAMETER_DESC paramDesc;
		reflector->GetInputParameterDesc(i, &paramDesc);

		D3D11_INPUT_ELEMENT_DESC elementDesc = {};
		elementDesc.SemanticName = paramDesc.SemanticName;
		elementDesc.SemanticIndex = paramDesc.SemanticIndex;
		elementDesc.InputSlot = 0;
		elementDesc.AlignedByteOffset = D3D11_APPEND_ALIGNED_ELEMENT;
		elementDesc.InputSlotClass = D3D11_INPUT_PER_VERTEX_DATA;
		elementDesc.InstanceDataStepRate = 0;

		// DXGI_FORMAT 매핑
		if (paramDesc.Mask == 1)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.Format = DXGI_FORMAT_R32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.Format = DXGI_FORMAT_R32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.Format = DXGI_FORMAT_R32_FLOAT;
		}
		else if (paramDesc.Mask <= 3)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.Format = DXGI_FORMAT_R32G32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.Format = DXGI_FORMAT_R32G32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.Format = DXGI_FORMAT_R32G32_FLOAT;
		}
		else if (paramDesc.Mask <= 7)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.Format = DXGI_FORMAT_R32G32B32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.Format = DXGI_FORMAT_R32G32B32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.Format = DXGI_FORMAT_R32G32B32_FLOAT;
		}
		else if (paramDesc.Mask <= 15)
		{
			if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_UINT32) elementDesc.Format = DXGI_FORMAT_R32G32B32A32_UINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_SINT32) elementDesc.Format = DXGI_FORMAT_R32G32B32A32_SINT;
			else if (paramDesc.ComponentType == D3D_REGISTER_COMPONENT_FLOAT32) elementDesc.Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
		}

		inputLayoutDesc.push_back(elementDesc);
	}

	Microsoft::WRL::ComPtr<ID3D11InputLayout> inputLayout;
	HR_T(RenderManager::Get().GetDevice()->CreateInputLayout(inputLayoutDesc.data(),
		(UINT)inputLayoutDesc.size(),
		_blob->GetBufferPointer(),
		_blob->GetBufferSize(),
		&inputLayout));

	return inputLayout;
}
