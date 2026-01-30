#include "SkyRenderer.h"

#include "Texture2D.h"
#include "Material.h"
#include "StaticMesh.h"

#include "rttr/registration"
#include "RenderManager.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<SkyRenderer>("SkyRenderer")
		(rttr::metadata("wrapper_type_name", "ObjPtr<SkyRenderer>"))
		.property("SkyTexture", &SkyRenderer::GetSkyTexture, &SkyRenderer::SetSkyTexture)
		.property("SkyIrr", &SkyRenderer::GetSkyIrr, &SkyRenderer::SetSkyIrr)
		.property("SkySpecular", &SkyRenderer::GetSkySpecular, &SkyRenderer::SetSkySpecular)
		.property("SkyBrdf", &SkyRenderer::GetSkyBrdf, &SkyRenderer::SetSkyBrdf);

	registration::class_<ObjPtr<SkyRenderer>>("ObjPtr<SkyRenderer>")
		.constructor<>(
			[]() {
				return Object::NewObject<SkyRenderer>();
			});
}

MMMEngine::ResPtr<MMMEngine::Texture2D>& MMMEngine::SkyRenderer::GetSkyTexture()
{
	/*if (!m_pSkyTexture)
		return L"";*/

	return m_pSkyTexture;
}

MMMEngine::ResPtr<MMMEngine::Texture2D>& MMMEngine::SkyRenderer::GetSkyIrr()
{
	return m_pSkyIrr;
}

MMMEngine::ResPtr<MMMEngine::Texture2D>& MMMEngine::SkyRenderer::GetSkySpecular()
{
	return m_pSkySpecular;
}

MMMEngine::ResPtr<MMMEngine::Texture2D>& MMMEngine::SkyRenderer::GetSkyBrdf()
{
	return m_pSkyBrdf;
}

void MMMEngine::SkyRenderer::SetSkyTexture(MMMEngine::ResPtr<MMMEngine::Texture2D>& _res)
{
	//auto tex = ResourceManager::Get().Load<Texture2D>(_path);
	m_pSkyTexture = _res;
	UpdateSRVs();
}

void MMMEngine::SkyRenderer::SetSkyIrr(MMMEngine::ResPtr<MMMEngine::Texture2D>& _res)
{
	//auto tex = ResourceManager::Get().Load<Texture2D>(_path);
	m_pSkyIrr = _res;
	UpdateSRVs();
}

void MMMEngine::SkyRenderer::SetSkySpecular(MMMEngine::ResPtr<MMMEngine::Texture2D>& _res)
{
	//auto tex = ResourceManager::Get().Load<Texture2D>(_path);
	m_pSkySpecular = _res;
	UpdateSRVs();
}

void MMMEngine::SkyRenderer::SetSkyBrdf(MMMEngine::ResPtr<MMMEngine::Texture2D>& _res)
{
	//auto tex = ResourceManager::Get().Load<Texture2D>(_path);
	m_pSkyBrdf = _res;
	UpdateSRVs();
}

void MMMEngine::SkyRenderer::Initialize()
{
	m_pSkyMaterial = std::make_shared<Material>();
	m_pSkyMaterial->AddProperty(L"_specular", m_pSkySpecular);
	m_pSkyMaterial->AddProperty(L"_irradiance", m_pSkyIrr);
	m_pSkyMaterial->AddProperty(L"_brdflut", m_pSkyBrdf);
	m_pSkyMaterial->AddProperty(L"_cubemap", m_pSkyTexture);

	m_pSkyMaterial->SetVShader(ResourceManager::Get().Load<VShader>(L"Shader/SkyBox/SkyBoxVertexShader.hlsl"));
	m_pSkyMaterial->SetPShader(ResourceManager::Get().Load<PShader>(L"Shader/SkyBox/SkyBoxPixelShader.hlsl"));

	m_pMesh = ResourceManager::Get()
		.Load<StaticMesh>(L"Shader/Resource/Default_Mesh/SkyBoxMesh_StaticMesh.staticmesh");

	renderIndex = RenderManager::Get().AddRenderer(this);
}

void MMMEngine::SkyRenderer::UnInitialize()
{
	RenderManager::Get().RemoveRenderer(renderIndex);

	// ShaderInfo 공용리소스 삭제
	for (auto& [prop, val] : m_pSkyMaterial->GetProperties())
		ShaderInfo::Get().RemoveGlobalPropVal(S_PBR, prop);
	m_pSkyMaterial.reset();
}

void MMMEngine::SkyRenderer::Render()
{
	// 유효성 확인
	if (!m_pMesh)
		return;

	// ShaderInfo 공용리소스 업데이트
	for (auto& [prop, val] : m_pSkyMaterial->GetProperties())
		ShaderInfo::Get().AddAllGlobalPropVal(prop, val);

	for (auto& [matIdx, meshIndices] : m_pMesh->meshGroupData) {

		for (const auto& idx : meshIndices) {
			RenderCommand command;
			auto& meshBuffer = m_pMesh->gpuBuffer.vertexBuffers[idx];
			auto& indicesBuffer = m_pMesh->gpuBuffer.indexBuffers[idx];

			command.vertexBuffer = meshBuffer.Get();
			command.indexBuffer = indicesBuffer.Get();
			command.material = m_pSkyMaterial;
			command.worldMatIndex = RenderManager::Get().AddMatrix(DirectX::SimpleMath::Matrix::Identity);
			command.indiciesSize = m_pMesh->indexSizes[idx];
			command.camDistance = 0.0f;

			std::wstring shaderPath = m_pSkyMaterial->GetPShader()->GetFilePath();
			RenderType type = ShaderInfo::Get().GetRenderType(shaderPath);

			RenderManager::Get().AddCommand(type, std::move(command));
		}
	}
}

void MMMEngine::SkyRenderer::UpdateSRVs()
{
	m_pSkyMaterial->SetProperty(L"_specular", m_pSkySpecular);
	m_pSkyMaterial->SetProperty(L"_irradiance", m_pSkyIrr);
	m_pSkyMaterial->SetProperty(L"_brdflut", m_pSkyBrdf);
	m_pSkyMaterial->SetProperty(L"_cubemap", m_pSkyTexture);
}
