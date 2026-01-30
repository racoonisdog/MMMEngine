#pragma once

#include "Export.h"
#include "RenderShared.h"

namespace MMMEngine {
	class RenderManager;
	class Material;
	class MMMENGINE_API RenderCommand
	{
	public:
		float camDistance;			// 카메라와의 거리 (Transculant용)

		ID3D11Buffer* vertexBuffer;	// 버텍스 버퍼
		ID3D11Buffer* indexBuffer;	// 인덱스 버퍼
		std::weak_ptr<Material> material;			// 메테리얼

		UINT indiciesSize = (UINT)-1;		// 인덱스 사이즈 (-1 나오면 안돼)
		int worldMatIndex = -1;		// 월드 매트릭스 인덱스 (-1이 나오면 절대안됨!!)
		int boneMatIndex = -1;		// 본 매트릭스 인덱스 (-1은 스킨드메시아님)
		uint32_t rendererID = UINT32_MAX;
	};
}

