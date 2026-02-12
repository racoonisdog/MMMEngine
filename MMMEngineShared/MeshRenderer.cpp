#include "MeshRenderer.h"
#include "RenderManager.h"
#include "RenderCommand.h"
#include "GameObject.h"
#include "Transform.h"
#include "ShaderInfo.h"
#include "PShader.h"
#include "Material.h"

#include "StaticMesh.h"
#include "rttr/registration.h"
#include "SkinRenderer.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<MeshRenderer>("MeshRenderer")
		(rttr::metadata("wrapper_type_name", "ObjPtr<MeshRenderer>"))
		.property("Mesh", &MeshRenderer::GetMesh, &MeshRenderer::SetMesh)
		//.property("Materials", &MeshRenderer::GetMaterial, &MeshRenderer::SetMaterial)
		.property("CastShadow", &MeshRenderer::GetCastShadow, &MeshRenderer::SetCastShadow)
		.property("ReceiveShadow", &MeshRenderer::GetReceiveShadow, &MeshRenderer::SetReceiveShadow)
		.property("DitherAlpha", &MeshRenderer::GetDitherAlpha, &MeshRenderer::SetDitherAlpha);

	registration::class_<ObjPtr<MeshRenderer>>("ObjPtr<MeshRenderer>")
		.constructor<>(
			[]() {
				return Object::NewObject<MeshRenderer>();
			})
		.method("Inject", &ObjPtr<MeshRenderer>::Inject);
}

void MMMEngine::MeshRenderer::SetMesh(ResPtr<StaticMesh> _mesh)
{
	mesh = _mesh;
}
 
bool MMMEngine::MeshRenderer::GetCastShadow()
{
	if (!mesh)
		return false;

	return castShadows;
}

void MMMEngine::MeshRenderer::SetCastShadow(bool _val)
{
	if (!mesh)
		return;

	castShadows = _val;
}

void MMMEngine::MeshRenderer::SetReceiveShadow(bool _val)
{
	if (!mesh)
		return;

	receiveShadows = _val;
}

bool MMMEngine::MeshRenderer::GetReceiveShadow()
{
	if (!mesh)
		return false;
	
	return receiveShadows;
}

std::vector<MMMEngine::ResPtr<MMMEngine::Material>>& MMMEngine::MeshRenderer::GetMaterial()
{
	return mesh->materials;
}

void MMMEngine::MeshRenderer::SetMaterial(std::vector<ResPtr<Material>>& _materials)
{
	if (!mesh)
		return;
	if (mesh->materials.size() != _materials.size())
		return;
	
	mesh->materials = _materials;
}

void MMMEngine::MeshRenderer::Initialize()
{
	renderIndex = RenderManager::Get().AddRenderer(this);
}

void MMMEngine::MeshRenderer::UnInitialize()
{
	RenderManager::Get().RemoveRenderer(renderIndex);
	mesh.reset();
}

void MMMEngine::MeshRenderer::Render()
{
	// 유효성 확인
	if (!mesh || !GetTransform())
		return;

	for (auto& [matIdx, meshIndices] : mesh->meshGroupData) {
		if (mesh->materials.empty())
			continue;

		auto& material = mesh->materials[matIdx];

		if (!material)
			continue;

		for (const auto& idx : meshIndices) {
			RenderCommand command;
			auto& meshBuffer = mesh->gpuBuffer.vertexBuffers[idx];
			auto& indicesBuffer = mesh->gpuBuffer.indexBuffers[idx];

			command.vertexBuffer = meshBuffer.Get();
			command.indexBuffer = indicesBuffer.Get();
			command.material = material;
			command.worldMatIndex = RenderManager::Get().AddMatrix(GetTransform()->GetWorldMatrix());
			command.indiciesSize = mesh->indexSizes[idx];
			command.rendererID = renderIndex;
			command.castShadow = castShadows;
			command.receiveShadow = receiveShadows;
			command.useDitherAlpha = (ditherAlpha < 1.0f);
			command.ditherAlpha = ditherAlpha;

			// TODO::CamDistance 보내줘야함!!
			command.camDistance = 0.0f;

			std::wstring shaderPath = material->GetPShader()->GetFilePath();
			RenderType type = ShaderInfo::Get().GetRenderType(shaderPath);

			RenderManager::Get().AddCommand(type, std::move(command));
		}
	}
}
