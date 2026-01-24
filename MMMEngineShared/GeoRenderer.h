#pragma once
#include "Export.h"
#include "RendererBase.h"

namespace MMMEngine {
	class MMMENGINE_API GeoRenderer : public RendererBase
	{
	private:
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_pInputLayout;		// 입력 레이아웃
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pTransBuffer;			// 트랜스폼 버퍼
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pLightBuffer;			// 라이트 버퍼


		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pBoneBuffer;				// 본 버퍼
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pShadowBuffer;			// 그림자 버퍼
	public:
		void Initialize() override;
		void Render() override;
	};
}


