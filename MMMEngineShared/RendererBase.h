#pragma once
#ifndef NOMINMAX
	#define NOMINMAX
#endif

#include "Export.h"
#include <d3d11_4.h>
#include <Material.h>
#include "ResourceManager.h"

namespace MMMEngine {
	class MeshRenderer;
	class MMMENGINE_API RendererBase
	{
	protected:
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pVertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pIndexBuffer;
		UINT m_IndicesSize;
		ResPtr<Material> m_pMaterial;

		Microsoft::WRL::ComPtr<ID3D11DeviceContext4> m_pDeviceContext;	// 디바이스 컨텍스트 참조
		DirectX::SimpleMath::Matrix m_worldMat = DirectX::SimpleMath::Matrix::Identity;

	public:
		RendererBase();

		void SetWorldMat(const DirectX::SimpleMath::Matrix& _mat) { m_worldMat = _mat; }

		void SetRenderData(
			Microsoft::WRL::ComPtr<ID3D11Buffer>& _vertex,
			Microsoft::WRL::ComPtr<ID3D11Buffer>& _index,
			UINT _indicesSize,
			std::shared_ptr<Material>& _material
		);

		virtual void Initialize() = 0;
		virtual void Render() = 0;
	};
}


