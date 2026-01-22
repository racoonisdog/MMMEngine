#include "RendererBase.h"
#include "RenderManager.h"

MMMEngine::RendererBase::RendererBase() {
	m_IndicesSize = 0;
	m_pDeviceContext = RenderManager::Get().m_pDeviceContext;
}

void MMMEngine::RendererBase::SetRenderData(Microsoft::WRL::ComPtr<ID3D11Buffer>& _vertex, Microsoft::WRL::ComPtr<ID3D11Buffer>& _index, UINT _indicesSize, std::shared_ptr<Material>& _material)
{
	// 인풋값 유효성 체크
	if (!_vertex || !_index)
		return;

	m_pVertexBuffer = _vertex;
	m_pIndexBuffer = _index;
	m_IndicesSize = _indicesSize;
	m_pMaterial = _material;
}
