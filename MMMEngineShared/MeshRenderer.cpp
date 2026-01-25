#include "MeshRenderer.h"
#include "StaticMesh.h"
#include "RenderManager.h"
#include "RenderCommand.h"
#include "Object.h"
#include "Transform.h"
#include "ShaderInfo.h"
#include "PShader.h"

#include "rttr/registration.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<MeshRenderer>("MeshRenderer")
		(rttr::metadata("wrapper_type", rttr::type::get<ObjPtr<MeshRenderer>>()))
		.property("Mesh", &MeshRenderer::GetMesh, &MeshRenderer::SetMesh);
		

	registration::class_<ObjPtr<MeshRenderer>>("ObjPtr<MeshRenderer>")
		.constructor<>(
			[]() {
				return Object::NewObject<MeshRenderer>();
			});

	type::register_wrapper_converter_for_base_classes<MMMEngine::ObjPtr<MeshRenderer>>();
}

MMMEngine::MeshRenderer::MeshRenderer()
{
	REGISTER_BEHAVIOUR_MESSAGE(Start);
	REGISTER_BEHAVIOUR_MESSAGE(Update);
}

void MMMEngine::MeshRenderer::SetMesh(ResPtr<StaticMesh>& _mesh)
{
	mesh = _mesh;
	Start();
}

void MMMEngine::MeshRenderer::Start()
{
	// 메시 없으면 리턴
	if (!mesh)
		return;

	// 메테리얼 인덱스별로 파일경로 캐싱
	for (int i = 0; i < mesh->materials.size(); ++i) {
		m_shaderPathMap[i] = mesh->materials[i]->GetPShader()->GetFilePath();
	}
}

void MMMEngine::MeshRenderer::Update()
{
	// 유효성 확인
	if (!mesh || !GetGameObject()->GetTransform())
		return;

	for (auto& [matIdx, meshIndices] : mesh->meshGroupData) {
		auto& material = mesh->materials[matIdx];

		for (const auto& idx : meshIndices) {
			RenderCommand command;
			auto& meshBuffer = mesh->gpuBuffer.vertexBuffers[idx];
			auto& indicesBuffer = mesh->gpuBuffer.indexBuffers[idx];

			command.vertexBuffer = meshBuffer.Get();
			command.indexBuffer = indicesBuffer.Get();
			command.material = material.get();
			command.worldMatIndex = RenderManager::Get().AddMatrix(GetGameObject()->GetTransform()->GetWorldMatrix());
			command.indiciesSize = mesh->indexSizes[idx];

			// TODO::CamDistance 보내줘야함!!
			command.camDistance = 0.0f;

			RenderType type = RenderType::R_GEOMETRY;

			auto it = m_shaderPathMap.find(idx);
			if (it != m_shaderPathMap.end()) {
				type = ShaderInfo::Get().GetRenderType(it->second);
			}

			RenderManager::Get().AddCommand(type, std::move(command));
		}
	}
}
