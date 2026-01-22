#pragma once

#include "Export.h"
#include "Resource.h"
#include <wrl/client.h>

#include <d3d11_4.h>

namespace MMMEngine {
	class MMMENGINE_API VShader : public Resource {
	public:
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_pVShader;
		Microsoft::WRL::ComPtr<ID3DBlob> m_pBlob;

		bool LoadFromFilePath(const std::wstring& filePath) override;
	};
}