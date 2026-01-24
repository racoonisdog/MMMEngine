#include "RenderStateGuard.h"

namespace MMMEngine::Utility
{
    RenderStateGuard::RenderStateGuard(ID3D11DeviceContext* ctx)
        : m_ctx(ctx)
    {
        if (!m_ctx)
            return;

        // Render Targets
        m_ctx->OMGetRenderTargets(1, &m_rtv, &m_dsv);

        // Blend
        m_ctx->OMGetBlendState(&m_blend, m_blendFactor, &m_sampleMask);

        // Depth
        m_ctx->OMGetDepthStencilState(&m_depth, &m_stencilRef);

        // Rasterizer
        m_ctx->RSGetState(&m_raster);

        // Viewports
        m_numViewports = D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE;
        m_ctx->RSGetViewports(&m_numViewports, m_viewports);
    }

    RenderStateGuard::~RenderStateGuard()
    {
        if (!m_ctx)
            return;

        // Restore
        m_ctx->OMSetRenderTargets(1, &m_rtv, m_dsv);
        m_ctx->OMSetBlendState(m_blend, m_blendFactor, m_sampleMask);
        m_ctx->OMSetDepthStencilState(m_depth, m_stencilRef);
        m_ctx->RSSetState(m_raster);
        m_ctx->RSSetViewports(m_numViewports, m_viewports);

        // Release
        if (m_rtv)    m_rtv->Release();
        if (m_dsv)    m_dsv->Release();
        if (m_blend)  m_blend->Release();
        if (m_depth)  m_depth->Release();
        if (m_raster) m_raster->Release();
    }
}
