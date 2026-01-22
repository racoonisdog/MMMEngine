#pragma once
#include "Export.h"
#include "Resource.h"
#include <wrl/client.h>
#include <d3d11_4.h>

namespace MMMEngine {
	class MMMENGINE_API PShader : public Resource
	{
		RTTR_ENABLE(Resource);
		RTTR_REGISTRATION_FRIEND
			friend class ResourceManager;
			friend class SceneManager;
			friend class Scene;
	public:
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pPShader;
		Microsoft::WRL::ComPtr<ID3DBlob> m_pBlob;

		bool LoadFromFilePath(const std::wstring& filePath) override;
	};
}


