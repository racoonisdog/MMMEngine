#pragma once
#include <d3d11.h>
#include <DirectXMath.h>
#include <wrl/client.h>
#include <vector>

using namespace DirectX;
using Microsoft::WRL::ComPtr;

namespace MMMEngine::Editor
{
    class EditorCamera;
	class EditorGridRenderer
	{
    public:
        EditorGridRenderer();
        ~EditorGridRenderer();

        bool Initialize(ID3D11Device* device);
        void Render(ID3D11DeviceContext* context,
            MMMEngine::Editor::EditorCamera& camera);

    private:
        struct Vertex
        {
            XMFLOAT3 position;
        };

        struct ViewProjBuffer
        {
            XMMATRIX viewProj;
        };

        struct CameraBuffer
        {
            XMFLOAT3 cameraPosition;
            float padding;
        };

        bool CreateShaders(ID3D11Device* device);
        bool CreateBuffers(ID3D11Device* device);

        ComPtr<ID3D11VertexShader> m_vertexShader;
        ComPtr<ID3D11PixelShader> m_pixelShader;
        ComPtr<ID3D11InputLayout> m_inputLayout;
        ComPtr<ID3D11Buffer> m_vertexBuffer;
        ComPtr<ID3D11Buffer> m_viewProjBuffer;
        ComPtr<ID3D11Buffer> m_cameraBuffer;
        ComPtr<ID3D11BlendState> m_blendState;
        ComPtr<ID3D11RasterizerState> m_rasterizerState;
        ComPtr<ID3D11DepthStencilState> m_depthStencilState;

        static constexpr float halfGridSize = 500.0f;
        static constexpr UINT vertexCount = 6;
	};
}