#include "SkeletalMesh.h"
#include <rttr/registration.h>

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<SkeletalMesh>("SkeletalMesh")
		.property("castShadows", &SkeletalMesh::castShadows)
		.property("receiveShadows", &SkeletalMesh::receiveShadows)
		.property("meshData", &SkeletalMesh::meshData)
		.property("meshGroupData", &SkeletalMesh::meshGroupData)
		.property("boneBuffer", &SkeletalMesh::boneBuffer)
		.property("offsetBuffer", &SkeletalMesh::offsetBuffer);
}

bool MMMEngine::SkeletalMesh::LoadFromFilePath(const std::wstring& _path)
{
	return true;
}
