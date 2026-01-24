#include "MeshRenderer.h"
#include "RendererBase.h"
#include "StaticMesh.h"
#include "RenderManager.h"
#include "GeoRenderer.h"
#include "Object.h"
#include "Transform.h"

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

MMMEngine::MeshRenderer::~MeshRenderer()
{
	// 렌더러 제거 명령
	for (auto& renderer : renderers) {
		if (auto locked = renderer.lock()) {
			RenderManager::Get().RemoveRenderer(RenderType::R_GEOMETRY, locked);
		}
	}
}

void MMMEngine::MeshRenderer::SetMesh(ResPtr<StaticMesh>& _mesh)
{
	mesh = _mesh;
	Start();
}

void MMMEngine::MeshRenderer::Start()
{
	// 유효성 확인
	if (!mesh || mesh->meshData.vertices.empty() || mesh->gpuBuffer.vertexBuffers.empty())
		return;

	for (auto& [matIdx, meshIndices] : mesh->meshGroupData) {
		auto& material = mesh->materials[matIdx];

		for (const auto& idx : meshIndices) {
			std::weak_ptr<RendererBase> renderer;

			renderer = RenderManager::Get().AddRenderer<GeoRenderer>(RenderType::R_GEOMETRY);

			auto& meshBuffer = mesh->gpuBuffer.vertexBuffers[idx];
			auto& indicesBuffer = mesh->gpuBuffer.indexBuffers[idx];
			UINT indicesSize = static_cast<UINT>(mesh->meshData.indices[idx].size());

			if (auto locked = renderer.lock()) {
				locked->SetRenderData(meshBuffer, indicesBuffer, indicesSize, material);
				renderers.push_back(renderer);
			}
		}
	}
}

void MMMEngine::MeshRenderer::Update()
{
	// 월드 매트릭스 전달
	for (auto& renderer : renderers) {
		if (auto locked = renderer.lock()) {
			if (auto transform = GetGameObject()->GetTransform())
				locked->SetWorldMat(transform->GetWorldMatrix());
		}
	}
}
