#include "GeoRenderer.h"
#include "RenderManager.h"
#include "RenderShared.h"
#include <RendererTools.h>

#include "ShaderInfo.h"
#include "VShader.h"
#include "PShader.h"

void MMMEngine::GeoRenderer::Initialize()
{
	// 디바이스 가져오기
	auto device = RenderManager::Get().GetDevice();

	m_pInputLayout = RenderManager::Get().GetDefaultInputLayout();

	// 버퍼 생성
	D3D11_BUFFER_DESC bd = {};
	bd.Usage = D3D11_USAGE_DEFAULT;
	bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
	bd.CPUAccessFlags = 0;

	bd.ByteWidth = sizeof(Render_TransformBuffer);
	HR_T(device->CreateBuffer(&bd, nullptr, &m_pTransBuffer));

	bd.ByteWidth = sizeof(Mesh_BoneBuffer);
	HR_T(device->CreateBuffer(&bd, nullptr, &m_pBoneBuffer));

	bd.ByteWidth = sizeof(Render_ShadowBuffer);
	HR_T(device->CreateBuffer(&bd, nullptr, &m_pShadowBuffer));
}

void MMMEngine::GeoRenderer::Render()
{
	// 버텍스 등록
	UINT stride = sizeof(Mesh_Vertex);
	UINT offset = 0;
	m_pDeviceContext->IASetInputLayout(m_pInputLayout.Get());
	m_pDeviceContext->IASetVertexBuffers(0, 1, m_pVertexBuffer.GetAddressOf(), &stride, &offset);
	m_pDeviceContext->IASetIndexBuffer(m_pIndexBuffer.Get(), DXGI_FORMAT_R32_UINT, offset);

	auto VS = m_pMaterial->GetVShader()->m_pVShader;
	auto PS = m_pMaterial->GetPShader()->m_pPShader;
	m_pDeviceContext->VSSetShader(VS.Get(), nullptr, 0);
	m_pDeviceContext->PSSetShader(PS.Get(), nullptr, 0);

	// 트랜스폼 등록
	Render_TransformBuffer transformBuffer;
	transformBuffer.mWorld = XMMatrixTranspose(m_worldMat);
	transformBuffer.mNormalMatrix = XMMatrixInverse(nullptr, m_worldMat);
	m_pDeviceContext->UpdateSubresource1(m_pTransBuffer.Get(), 0, nullptr, &transformBuffer, 0, 0, D3D11_COPY_DISCARD);
	m_pDeviceContext->VSSetConstantBuffers(1, 1, m_pTransBuffer.GetAddressOf());

	// 메테리얼 등록 (Texture2D만 받는다)
	for (auto& [prop, val] : m_pMaterial->GetProperties()) {
		int idx = ShaderInfo::Get().PropertyToIdx(ShaderType::S_PBR, prop);

		if (auto tex = std::get_if<ResPtr<Texture2D>>(&val)) {
			if (*tex) {
				ID3D11ShaderResourceView* srv = (*tex)->m_pSRV.Get();
				m_pDeviceContext->PSSetShaderResources(idx, 1, &srv);
			}
		}
	}

	// 드로우콜
	m_pDeviceContext->DrawIndexed(m_IndicesSize, 0, 0);
}
