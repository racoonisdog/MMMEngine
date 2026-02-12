#pragma once

#include "Export.h"
#include "RenderShared.h"
#include "ResourceManager.h"

namespace MMMEngine {
	class RenderManager;
	class Material;
	class MMMENGINE_API RenderCommand
	{
	public:
		float camDistance;			// 카메라와의 거리 (Transculant용)

		ID3D11Buffer* vertexBuffer;	// 버텍스 버퍼
		ID3D11Buffer* indexBuffer;	// 인덱스 버퍼
		ResPtr<Material> material;			// 메테리얼

		UINT indiciesSize = (UINT)-1;		// 인덱스 사이즈 (-1 나오면 안돼)
		int worldMatIndex = -1;		// 월드 매트릭스 인덱스 (-1이 나오면 절대안됨!!)
		
		uint32_t rendererID = UINT32_MAX;

		Mesh_BoneBuffer* offsetBuffer	= nullptr;	// 본 오프셋 버퍼
		Mesh_BoneBuffer* animBuffer		= nullptr;	// 본 애니메이션 버퍼

		bool castShadow = true;
		bool receiveShadow = true;

		// Particle rendering overrides
		bool useParticleAlpha = false;
		float particleAlpha = 1.0f;

		// MeshRenderer dithering (no alpha pass: control transparency via dither)
		bool useDitherAlpha = false;
		float ditherAlpha = 1.0f;
	};
}

