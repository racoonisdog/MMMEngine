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

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<MeshRenderer>("MeshRenderer")
		(rttr::metadata("wrapper_type_name", "ObjPtr<MeshRenderer>"))
		.property("Mesh", &MeshRenderer::GetMesh, &MeshRenderer::SetMesh);

	registration::class_<ObjPtr<MeshRenderer>>("ObjPtr<MeshRenderer>")
		.constructor<>(
			[]() {
				return Object::NewObject<MeshRenderer>();
			})
		.method("Inject", &ObjPtr<MeshRenderer>::Inject);
}

void MMMEngine::MeshRenderer::SetMesh(ResPtr<StaticMesh>& _mesh)
{
	mesh = _mesh;
}
 
void MMMEngine::MeshRenderer::Initialize()
{
	renderIndex = RenderManager::Get().AddRenderer(this);
}

void MMMEngine::MeshRenderer::UnInitialize()
{
	RenderManager::Get().RemoveRenderer(renderIndex);
}

void MMMEngine::MeshRenderer::Init()
{
	
}

void MMMEngine::MeshRenderer::Render()
{
	// 유효성 확인
	if (!mesh || !GetTransform())
		return;

	for (auto& [matIdx, meshIndices] : mesh->meshGroupData) {
		auto& material = mesh->materials[matIdx];

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

			// TODO::CamDistance 보내줘야함!!
			command.camDistance = 0.0f;

			std::wstring shaderPath = material->GetPShader()->GetFilePath();
			RenderType type = ShaderInfo::Get().GetRenderType(shaderPath);

			RenderManager::Get().AddCommand(type, std::move(command));
		}
	}
}
