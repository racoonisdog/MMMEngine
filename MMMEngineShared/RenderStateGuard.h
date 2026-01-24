#pragma once
#include <d3d11.h>
#include "Export.h"

namespace MMMEngine::Utility
{
    class MMMENGINE_API RenderStateGuard
    {
    public:
        explicit RenderStateGuard(ID3D11DeviceContext* ctx);
        ~RenderStateGuard();

        RenderStateGuard(const RenderStateGuard&) = delete;
        RenderStateGuard& operator=(const RenderStateGuard&) = delete;

    private:
        ID3D11DeviceContext* m_ctx = nullptr;

        ID3D11RenderTargetView* m_rtv = nullptr;
        ID3D11DepthStencilView* m_dsv = nullptr;

        ID3D11BlendState* m_blend = nullptr;
        FLOAT                    m_blendFactor[4]{};
        UINT                     m_sampleMask = 0;

        ID3D11DepthStencilState* m_depth = nullptr;
        UINT                     m_stencilRef = 0;

        ID3D11RasterizerState* m_raster = nullptr;

        D3D11_VIEWPORT           m_viewports[D3D11_VIEWPORT_AND_SCISSORRECT_OBJECT_COUNT_PER_PIPELINE]{};
        UINT                     m_numViewports = 0;
    };
}
