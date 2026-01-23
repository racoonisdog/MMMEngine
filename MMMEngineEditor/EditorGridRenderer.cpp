#include "EditorGridRenderer.h"
#include "EditorShader.h"
#include "EditorCamera.h"
#include <d3dcompiler.h>
#include <fstream>
#define NOMINMAX
#include "SimpleMath.h"

#pragma comment(lib, "d3dcompiler.lib")

using namespace DirectX::SimpleMath;

MMMEngine::Editor::EditorGridRenderer::EditorGridRenderer()
{
}

MMMEngine::Editor::EditorGridRenderer::~EditorGridRenderer()
{
}

bool MMMEngine::Editor::EditorGridRenderer::Initialize(ID3D11Device* device)
{
    if (!CreateShaders(device))
        return false;

    if (!CreateBuffers(device))
        return false;

    // Create blend state for alpha blending
    D3D11_BLEND_DESC blendDesc = {};
    blendDesc.RenderTarget[0].BlendEnable = TRUE;
    blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
    blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
    blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
    blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ZERO;
    blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
    blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

    HRESULT hr = device->CreateBlendState(&blendDesc, m_blendState.GetAddressOf());
    if (FAILED(hr))
        return false;

    // Create rasterizer state
    D3D11_RASTERIZER_DESC rastDesc = {};
    rastDesc.FillMode = D3D11_FILL_SOLID;
    rastDesc.CullMode = D3D11_CULL_NONE;
    rastDesc.FrontCounterClockwise = FALSE;
    rastDesc.DepthClipEnable = TRUE;

    hr = device->CreateRasterizerState(&rastDesc, m_rasterizerState.GetAddressOf());
    if (FAILED(hr))
        return false;

    return true;
}

void MMMEngine::Editor::EditorGridRenderer::Render(ID3D11DeviceContext* context, MMMEngine::Editor::EditorCamera& camera)
{
    // Update ViewProj constant buffer
    D3D11_MAPPED_SUBRESOURCE mappedResource;
    HRESULT hr = context->Map(m_viewProjBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        ViewProjBuffer* vpBuffer = reinterpret_cast<ViewProjBuffer*>(mappedResource.pData);

        Matrix view = camera.GetViewMatrix();
        Matrix proj = camera.GetProjMatrix();
        Matrix viewProj = view * proj;

        vpBuffer->viewProj = viewProj.Transpose();
        context->Unmap(m_viewProjBuffer.Get(), 0);
    }

    // Update Camera constant buffer
    hr = context->Map(m_cameraBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mappedResource);
    if (SUCCEEDED(hr))
    {
        CameraBuffer* camBuffer = reinterpret_cast<CameraBuffer*>(mappedResource.pData);
        camBuffer->cameraPosition = camera.GetPosition();
        camBuffer->padding = 0.0f;
        context->Unmap(m_cameraBuffer.Get(), 0);
    }

    // Set render states
    float blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    context->OMSetBlendState(m_blendState.Get(), blendFactor, 0xFFFFFFFF);
    context->OMSetDepthStencilState(m_depthStencilState.Get(), 0);
    context->RSSetState(m_rasterizerState.Get());

    // Set shaders
    context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
    context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

    // Set input layout
    context->IASetInputLayout(m_inputLayout.Get());

    // Set constant buffers
    context->VSSetConstantBuffers(0, 1, m_viewProjBuffer.GetAddressOf());
    context->PSSetConstantBuffers(0, 1, m_cameraBuffer.GetAddressOf());

    // Set vertex buffer
    UINT stride = sizeof(Vertex);
    UINT offset = 0;
    context->IASetVertexBuffers(0, 1, m_vertexBuffer.GetAddressOf(), &stride, &offset);
    context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    // Draw
    context->Draw(vertexCount, 0);
}

bool MMMEngine::Editor::EditorGridRenderer::CreateShaders(ID3D11Device* device)
{
    UINT compileFlags = D3DCOMPILE_ENABLE_STRICTNESS;
#ifdef _DEBUG
    compileFlags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

    ComPtr<ID3DBlob> vsBlob;
    ComPtr<ID3DBlob> psBlob;
    ComPtr<ID3DBlob> errorBlob;

    // Compile Vertex Shader from string
    HRESULT hr = D3DCompile(
        MMMEngine::EditorShader::g_shader_grid,
        strlen(MMMEngine::EditorShader::g_shader_grid),
        nullptr,
        nullptr,
        nullptr,
        "VS_Main",
        "vs_5_0",
        compileFlags,
        0,
        vsBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA("Vertex Shader Compilation Error:\n");
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        return false;
    }

    // Compile Pixel Shader from string
    hr = D3DCompile(
        MMMEngine::EditorShader::g_shader_grid,
        strlen(MMMEngine::EditorShader::g_shader_grid),
        nullptr,
        nullptr,
        nullptr,
        "PS_Main",
        "ps_5_0",
        compileFlags,
        0,
        psBlob.GetAddressOf(),
        errorBlob.GetAddressOf()
    );

    if (FAILED(hr))
    {
        if (errorBlob)
        {
            OutputDebugStringA("Pixel Shader Compilation Error:\n");
            OutputDebugStringA((char*)errorBlob->GetBufferPointer());
        }
        return false;
    }

    // Create shaders
    hr = device->CreateVertexShader(
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        nullptr,
        m_vertexShader.GetAddressOf()
    );

    if (FAILED(hr))
        return false;

    hr = device->CreatePixelShader(
        psBlob->GetBufferPointer(),
        psBlob->GetBufferSize(),
        nullptr,
        m_pixelShader.GetAddressOf()
    );

    if (FAILED(hr))
        return false;

    // Create input layout
    D3D11_INPUT_ELEMENT_DESC layout[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 }
    };

    hr = device->CreateInputLayout(
        layout,
        _countof(layout),
        vsBlob->GetBufferPointer(),
        vsBlob->GetBufferSize(),
        m_inputLayout.GetAddressOf()
    );

    if (FAILED(hr))
        return false;

    return true;
}

bool MMMEngine::Editor::EditorGridRenderer::CreateBuffers(ID3D11Device* device)
{
    // Vertex buffer
    std::vector<Vertex> vertices =
    {
        { Vector3(-halfGridSize, 0.0f, -halfGridSize) },
        { Vector3(halfGridSize, 0.0f, -halfGridSize) },
        { Vector3(halfGridSize, 0.0f,  halfGridSize) },
        { Vector3(-halfGridSize, 0.0f, -halfGridSize) },
        { Vector3(halfGridSize, 0.0f,  halfGridSize) },
        { Vector3(-halfGridSize, 0.0f,  halfGridSize) }
    };

    D3D11_BUFFER_DESC vbDesc = {};
    vbDesc.Usage = D3D11_USAGE_IMMUTABLE;
    vbDesc.ByteWidth = sizeof(Vertex) * static_cast<UINT>(vertices.size());
    vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;

    D3D11_SUBRESOURCE_DATA vbData = {};
    vbData.pSysMem = vertices.data();

    HRESULT hr = device->CreateBuffer(&vbDesc, &vbData, m_vertexBuffer.GetAddressOf());
    if (FAILED(hr))
        return false;

    // ViewProj constant buffer
    D3D11_BUFFER_DESC vpCbDesc = {};
    vpCbDesc.Usage = D3D11_USAGE_DYNAMIC;
    vpCbDesc.ByteWidth = sizeof(ViewProjBuffer);
    vpCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    vpCbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateBuffer(&vpCbDesc, nullptr, m_viewProjBuffer.GetAddressOf());
    if (FAILED(hr))
        return false;

    // Camera constant buffer
    D3D11_BUFFER_DESC camCbDesc = {};
    camCbDesc.Usage = D3D11_USAGE_DYNAMIC;
    camCbDesc.ByteWidth = sizeof(CameraBuffer);
    camCbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
    camCbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

    hr = device->CreateBuffer(&camCbDesc, nullptr, m_cameraBuffer.GetAddressOf());
    if (FAILED(hr))
        return false;

    return true;
}
