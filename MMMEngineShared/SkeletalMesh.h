#pragma once
#include "Export.h"
#include "Resource.h"
#include "RenderShared.h"

namespace MMMEngine {
	class MMMENGINE_API SkeletalMesh : public Resource
	{
		RTTR_ENABLE(Resource);
		RTTR_REGISTRATION_FRIEND
			friend class ResourceManager;
			friend class SceneManager;
			friend class Scene;
	public:
		// 메시 데이터
		MeshData meshData;
		// GPU 버퍼
		MeshGPU gpuBuffer;
		// 메테리얼
		std::vector<ResPtr<Material>> materials;
		// 메시 그룹 <MatIdx, MeshIdx>
		std::unordered_map<UINT, std::vector<UINT>> meshGroupData;

		// 본 버퍼
		Mesh_BoneBuffer boneBuffer;
		Mesh_BoneBuffer offsetBuffer;

		// 애니메이션 클립 목록


		bool castShadows = true;
		bool receiveShadows = true;

		// TODO::직렬화 시켜야함
		bool LoadFromFilePath(const std::wstring& _path) override;
	};
}


