#include "RenderShared.h"
#include "rttr/registration.h"

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<MMMEngine::Mesh_Vertex>("Mesh_Vertex")
		.constructor<>()
		.property("Pos", &Mesh_Vertex::Pos)
		.property("Normal", &Mesh_Vertex::Normal)
		.property("Tangent", &Mesh_Vertex::Tangent)
		.property("UV", &Mesh_Vertex::UV)
		.property("BoneIndices", &Mesh_Vertex::BoneIndices)
		.property("BoneWeights", &Mesh_Vertex::BoneWeights);

	auto reg = registration::class_<MMMEngine::Mesh_BoneBuffer>("Mesh_BoneBuffer")
		.constructor<>()
		.property("BoneMat", &Mesh_BoneBuffer::BoneMat);

	registration::class_<MMMEngine::MeshData>("MeshData")
		.property("vertices", &MeshData::vertices)
		.property("indices", &MeshData::indices);
}