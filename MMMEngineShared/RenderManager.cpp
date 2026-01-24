#include "RenderManager.h"
#include "GameObject.h"
#include "Transform.h"
//#include <EditorCamera.h>
#include <RendererTools.h>
#include "VShader.h"
#include "PShader.h"
#include "ResourceManager.h"

DEFINE_SINGLETON(MMMEngine::RenderManager)

using namespace Microsoft::WRL;
using namespace DirectX::SimpleMath;

namespace MMMEngine {

	RenderManager::RenderManager()
	{
		m_worldMatrix = Matrix::Identity;
		m_viewMatrix = Matrix::Identity;
		m_projMatrix = Matrix::Identity;
	}

	void RenderManager::StartUp(HWND _hwnd, UINT _ClientWidth, UINT _ClientHeight)
	{
		// 디바이스 생성
		ComPtr<ID3D11Device> device;
		D3D_FEATURE_LEVEL featureLevel;
		D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL,
			0, nullptr, 0, D3D11_SDK_VERSION,
			device.GetAddressOf(), &featureLevel, nullptr);

		HR_T(device.As(&m_pDevice));

		// hWnd 등록
		assert(_hwnd != nullptr && "RenderPipe::Initialize : hWnd must not be nullptr!!");
		m_hWnd = _hwnd;

		// 클라이언트 사이즈 등록
		m_clientWidth = _ClientWidth;
		m_clientHeight = _ClientHeight;

		// 인스턴스 초기화 뭉탱이
		InitD3D();
		Start();
	}
	void RenderManager::InitD3D()
	{
		// 스왑체인 속성설정 생성
		DXGI_SWAP_CHAIN_DESC1 swapDesc = {};
		swapDesc.BufferCount = 2;
		swapDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapDesc.Width = m_clientWidth;
		swapDesc.Height = m_clientHeight;
		swapDesc.SampleDesc.Count = 1;		// MSAA
		swapDesc.SampleDesc.Quality = 0;	// MSAA 품질수준

		// DXGI 디바이스
		ComPtr<IDXGIDevice> dxgiDevice;
		m_pDevice.As(&dxgiDevice);

		ComPtr<IDXGIAdapter> adapter;
		dxgiDevice->GetAdapter(&adapter);

		// 팩토리 생성
		ComPtr<IDXGIFactory2> dxgiFactory;
		adapter->GetParent(IID_PPV_ARGS(&dxgiFactory));

		// 스왑체인 생성
		Microsoft::WRL::ComPtr<IDXGISwapChain1> swapChain;
		HR_T(dxgiFactory->CreateSwapChainForHwnd(m_pDevice.Get(), m_hWnd, &swapDesc,
			nullptr, nullptr, swapChain.GetAddressOf()));
		HR_T(swapChain.As(&m_pSwapChain));

		// 컨텍스트 생성
		ComPtr<ID3D11DeviceContext3> context;
		m_pDevice->GetImmediateContext3(context.GetAddressOf());
		HR_T(context.As(&m_pDeviceContext));

		// 스왑체인 렌더타겟 생성
		ID3D11Texture2D1* backBuffer;
		HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D1), (void**)&backBuffer));
		HR_T(m_pDevice->CreateRenderTargetView1(backBuffer, nullptr, m_pRenderTargetView.GetAddressOf()));
		backBuffer->Release();

		// 뷰포트 설정
		m_swapViewport = {};
		m_swapViewport.TopLeftX = 0.0f;
		m_swapViewport.TopLeftY = 0.0f;
		m_swapViewport.Width = static_cast<float>(m_clientWidth);
		m_swapViewport.Height = static_cast<float>(m_clientHeight);
		m_swapViewport.MinDepth = 0.0f;
		m_swapViewport.MaxDepth = 1.0f;

		// X스 텍스쳐 생성
		D3D11_TEXTURE2D_DESC1 depthDesc = {};
		depthDesc.Width = m_clientWidth;
		depthDesc.Height = m_clientHeight;
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.SampleDesc.Quality = 0;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		depthDesc.CPUAccessFlags = 0;
		depthDesc.MiscFlags = 0;

		HR_T(m_pDevice->CreateTexture2D1(&depthDesc, nullptr, m_pDepthStencilBuffer.GetAddressOf()));

		// X스스탠실 뷰 생성
		D3D11_DEPTH_STENCIL_VIEW_DESC dsv = {};
		dsv.Format = depthDesc.Format;
		dsv.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsv.Texture2D.MipSlice = 0;
		HR_T(m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), &dsv, m_pDepthStencilView.GetAddressOf()));

		// 래스터라이저 속성 생성
		D3D11_RASTERIZER_DESC2 defaultRsDesc = {};
		defaultRsDesc.FillMode = D3D11_FILL_SOLID;
		defaultRsDesc.CullMode = D3D11_CULL_BACK;
		defaultRsDesc.FrontCounterClockwise = FALSE;
		defaultRsDesc.DepthBias = 0;
		defaultRsDesc.DepthBiasClamp = 0.0f;
		defaultRsDesc.SlopeScaledDepthBias = 0.0f;
		defaultRsDesc.DepthClipEnable = TRUE;
		defaultRsDesc.ScissorEnable = FALSE;
		defaultRsDesc.MultisampleEnable = FALSE;
		defaultRsDesc.AntialiasedLineEnable = FALSE;
		HR_T(m_pDevice->CreateRasterizerState2(&defaultRsDesc, m_pDefaultRS.GetAddressOf()));

		// 블랜드 스테이트 생성
		D3D11_BLEND_DESC1 blendDesc = {};
		blendDesc.AlphaToCoverageEnable = FALSE;
		blendDesc.IndependentBlendEnable = FALSE;
		blendDesc.RenderTarget[0].BlendEnable = FALSE;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		HR_T(m_pDevice->CreateBlendState1(&blendDesc, m_pDefaultBS.GetAddressOf()));
		assert(m_pDefaultBS && "RenderPipe::InitD3D : defaultBS not initialized!!");

		// 레스터라이저 스테이트 생성
		D3D11_RASTERIZER_DESC2 rsDesc = {};
		rsDesc.FillMode = D3D11_FILL_SOLID;
		rsDesc.CullMode = D3D11_CULL_BACK;
		rsDesc.FrontCounterClockwise = FALSE;
		rsDesc.DepthBias = 0;
		rsDesc.DepthBiasClamp = 0.0f;
		rsDesc.SlopeScaledDepthBias = 0.0f;
		rsDesc.DepthClipEnable = TRUE;
		rsDesc.ScissorEnable = FALSE;
		rsDesc.MultisampleEnable = FALSE;
		rsDesc.AntialiasedLineEnable = FALSE;
		HR_T(m_pDevice->CreateRasterizerState2(&rsDesc, m_pDefaultRS.GetAddressOf()));
		assert(m_pDefaultRS && "RenderPipe::InitD3D : defaultRS not initialized!!");
	
		// === Scene 렌더타겟 초기화 ===
		D3D11_TEXTURE2D_DESC1 sceneColorDesc = {};
		sceneColorDesc.Width = m_clientWidth;
		sceneColorDesc.Height = m_clientHeight;
		sceneColorDesc.MipLevels = 1;
		sceneColorDesc.ArraySize = 1;
		sceneColorDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR 지원 포맷
		sceneColorDesc.SampleDesc.Count = 1;
		sceneColorDesc.SampleDesc.Quality = 0;
		sceneColorDesc.Usage = D3D11_USAGE_DEFAULT;
		sceneColorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		// Scene 컬러 텍스처 생성
		HR_T(m_pDevice->CreateTexture2D1(&sceneColorDesc, nullptr, m_pSceneTexture.GetAddressOf()));

		// RTV 생성
		HR_T(m_pDevice->CreateRenderTargetView1(m_pSceneTexture.Get(), nullptr, m_pSceneRTV.GetAddressOf()));

		// SRV 생성
		HR_T(m_pDevice->CreateShaderResourceView1(m_pSceneTexture.Get(), nullptr, m_pSceneSRV.GetAddressOf()));

		// Depth/Stencil 버퍼 생성
		D3D11_TEXTURE2D_DESC1 sceneDepthDesc = {};
		sceneDepthDesc.Width = m_clientWidth;
		sceneDepthDesc.Height = m_clientHeight;
		sceneDepthDesc.MipLevels = 1;
		sceneDepthDesc.ArraySize = 1;
		sceneDepthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		sceneDepthDesc.SampleDesc.Count = 1;
		sceneDepthDesc.SampleDesc.Quality = 0;
		sceneDepthDesc.Usage = D3D11_USAGE_DEFAULT;
		sceneDepthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		sceneDepthDesc.CPUAccessFlags = 0;
		sceneDepthDesc.MiscFlags = 0;

		HR_T(m_pDevice->CreateTexture2D1(&sceneDepthDesc, nullptr, m_pSceneDSB.GetAddressOf()));

		// DSV 생성
		HR_T(m_pDevice->CreateDepthStencilView(m_pSceneDSB.Get(), nullptr, m_pSceneDSV.GetAddressOf()));

		m_sceneWidth = m_clientWidth;
		m_sceneHeight = m_clientHeight;

		// 씬 뷰포트 설정
		m_sceneViewport.TopLeftX = 0.0f;
		m_sceneViewport.TopLeftY = 0.0f;
		m_sceneViewport.Width = static_cast<float>(m_sceneWidth);
		m_sceneViewport.Height = static_cast<float>(m_sceneHeight);
		m_sceneViewport.MinDepth = 0.0f;
		m_sceneViewport.MaxDepth = 1.0f;

		// 기본 VSShader 생성
		m_pDefaultVSShader = ResourceManager::Get().Load<VShader>(L"Shader/PBR/VS/SkeletalVertexShader.hlsl");
		m_pDefaultPSShader = ResourceManager::Get().Load<PShader>(L"Shader/PBR/PS/BRDFShader.hlsl");

		// 기본 InputLayout 생성
		D3D11_INPUT_ELEMENT_DESC layout[] = {
	   { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	   { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	   { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	   { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	   { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	   { "BONEINDEX", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
	   { "BONEWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		HR_T(m_pDevice->CreateInputLayout(
			layout, ARRAYSIZE(layout), m_pDefaultVSShader->m_pBlob->GetBufferPointer(),
			m_pDefaultVSShader->m_pBlob->GetBufferSize(), m_pDefaultInputLayout.GetAddressOf()
		));

		// 캠 버퍼 생성
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;

		bd.ByteWidth = sizeof(Render_CamBuffer);
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, m_pCambuffer.GetAddressOf()));

	}
	void RenderManager::ShutDown()
	{
		// COM객체 초기화
		m_pDevice->Release();
		m_pDeviceContext->Release();
		m_pSwapChain->Release();


		// 변수 초기화
		while (!m_initQueue.empty())
			m_initQueue.pop();

		m_worldMatrix = Matrix::Identity;
		m_viewMatrix = Matrix::Identity;
		m_projMatrix = Matrix::Identity;
	}
	void RenderManager::Start()
	{
		// 버퍼 기본색상
		m_ClearColor = DirectX::SimpleMath::Vector4(0.45f, 0.55f, 0.60f, 1.00f);
	}

	void RenderManager::SetWorldMatrix(DirectX::SimpleMath::Matrix& _world)
	{
		m_worldMatrix = _world;
	}

	void RenderManager::SetViewMatrix(DirectX::SimpleMath::Matrix& _view)
	{
		m_viewMatrix = _view;
	}

	void RenderManager::SetProjMatrix(DirectX::SimpleMath::Matrix& _proj)
	{
		m_projMatrix = _proj;
	}

	void RenderManager::ResizeSwapChainSize(int width, int height)
	{
		m_clientWidth = width;
		m_clientHeight = height;

		// RTV 등록해제
		m_pDeviceContext->OMSetRenderTargets(0, nullptr, nullptr);

		// 기존 RTV/DSV 해제
		if (m_pRenderTargetView) m_pRenderTargetView->Release();
		if (m_pDepthStencilView) m_pDepthStencilView->Release();
		if (m_pDepthStencilBuffer) m_pDepthStencilBuffer->Release();

		// ResizeBuffers 호출
		HR_T(m_pSwapChain->ResizeBuffers(
			0,
			static_cast<UINT>(width),
			static_cast<UINT>(height),
			DXGI_FORMAT_UNKNOWN,
			0
		));

		// 새 백버퍼 가져오기
		ID3D11Texture2D1* buffer;
		HR_T(m_pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D1), (void**)&buffer));

		// 새 RTV 생성
		HR_T(m_pDevice->CreateRenderTargetView1(buffer, nullptr, m_pRenderTargetView.GetAddressOf()));
		buffer->Release();

		// Depth/Stencil 버퍼 생성
		D3D11_TEXTURE2D_DESC1 depthDesc = {};
		depthDesc.Width = width;
		depthDesc.Height = height;
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.SampleDesc.Quality = 0;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		HR_T(m_pDevice->CreateTexture2D1(&depthDesc, nullptr, m_pDepthStencilBuffer.GetAddressOf()));
		HR_T(m_pDevice->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), nullptr, m_pDepthStencilView.GetAddressOf()));
	}

	void RenderManager::ResizeSceneSize(int _width, int _height, int _sceneWidth, int _sceneHeight)
	{
		m_sceneWidth = _sceneWidth;
		m_sceneHeight = _sceneHeight;

		// 기존 리소스 해제
		if (m_pSceneRTV) { m_pSceneRTV->Release();      m_pSceneRTV = nullptr; }
		if (m_pSceneTexture) { m_pSceneTexture->Release();  m_pSceneTexture = nullptr; }
		if (m_pSceneSRV) { m_pSceneSRV->Release();      m_pSceneSRV = nullptr; }
		if (m_pSceneDSV) { m_pSceneDSV->Release();      m_pSceneDSV = nullptr; }
		if (m_pSceneDSB) { m_pSceneDSB->Release();		m_pSceneDSB = nullptr; }

		// 컬러 텍스처 설명
		D3D11_TEXTURE2D_DESC1 colorDesc = {};
		colorDesc.Width = _width;
		colorDesc.Height = _height;
		colorDesc.MipLevels = 1;
		colorDesc.ArraySize = 1;
		colorDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT; // HDR 지원 포맷
		colorDesc.SampleDesc.Count = 1;
		colorDesc.SampleDesc.Quality = 0;
		colorDesc.Usage = D3D11_USAGE_DEFAULT;
		colorDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

		// Scene 컬러 텍스처 생성
		HR_T(m_pDevice->CreateTexture2D1(&colorDesc, nullptr, m_pSceneTexture.GetAddressOf()));

		// RTV 생성
		HR_T(m_pDevice->CreateRenderTargetView1(m_pSceneTexture.Get(), nullptr, m_pSceneRTV.GetAddressOf()));

		// SRV 생성 (쉐이더에서 샘플링 가능)
		HR_T(m_pDevice->CreateShaderResourceView1(m_pSceneTexture.Get(), nullptr, m_pSceneSRV.GetAddressOf()));

		// Depth/Stencil 버퍼 설명
		D3D11_TEXTURE2D_DESC1 depthDesc = {};
		depthDesc.Width = _width;
		depthDesc.Height = _height;
		depthDesc.MipLevels = 1;
		depthDesc.ArraySize = 1;
		depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		depthDesc.SampleDesc.Count = 1;
		depthDesc.SampleDesc.Quality = 0;
		depthDesc.Usage = D3D11_USAGE_DEFAULT;
		depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		// Depth/Stencil 텍스처 생성
		HR_T(m_pDevice->CreateTexture2D1(&depthDesc, nullptr, m_pSceneDSB.GetAddressOf()));

		// DSV 생성
		HR_T(m_pDevice->CreateDepthStencilView(m_pSceneDSB.Get(), nullptr, m_pSceneDSV.GetAddressOf()));
		
		float sceneAspect = (float)_sceneWidth / (float)_sceneHeight;
		float backAspect = (float)_width / (float)_height;

		float drawW, drawH;

		if (backAspect > sceneAspect) {
			drawH = _height;
			drawW = _width * sceneAspect;
		}
		else {
			drawW = _width;
			drawH = _width / sceneAspect;
		}

		float offsetX = (_width - drawW) * 0.5f;
		float offsetY = (_height - drawH) * 0.5f;

		// 뷰포트 갱신
		m_sceneViewport.Width = drawW;
		m_sceneViewport.Height = drawH;
		m_sceneViewport.MinDepth = 0.0f;
		m_sceneViewport.MaxDepth = 1.0f;
		m_sceneViewport.TopLeftX = offsetX;
		m_sceneViewport.TopLeftY = offsetY;
	}

	void RenderManager::BeginFrame()
	{
		// Clear
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView.Get(), m_backColor);
		m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
	}

	void RenderManager::Render()
	{
		// Init Queue 처리
		while (!m_initQueue.empty()) {
			auto& renderer = m_initQueue.front();
			
			renderer->Initialize();
			m_initQueue.pop();
		}

		// Clear
		m_pDeviceContext->ClearRenderTargetView(m_pSceneRTV.Get(), m_backColor);
		m_pDeviceContext->ClearDepthStencilView(m_pSceneDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// 캠 버퍼 업데이트
		m_camMat.camPos = XMMatrixInverse(nullptr, m_viewMatrix).r[3];
		m_camMat.mView = m_viewMatrix;
		m_camMat.mProjection = m_projMatrix;

		// 리소스 업데이트
		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);

		// 기본 렌더셋팅
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->PSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());

		m_pDeviceContext->RSSetViewports(1, &m_sceneViewport);
		m_pDeviceContext->RSSetState(m_pDefaultRS.Get());
		m_pDeviceContext->OMSetRenderTargets(1, reinterpret_cast<ID3D11RenderTargetView* const*>(m_pSceneRTV.GetAddressOf()), m_pSceneDSV.Get());

		// RenderPass
		for (const auto& pass : m_Passes) {
			for (const auto& renderer : pass.second) {
				renderer->Render();
			}
		}
		
		// 씬렌더 해제
		m_pDeviceContext->RSSetViewports(1, &m_swapViewport);
		m_pDeviceContext->OMSetRenderTargets(1, reinterpret_cast<ID3D11RenderTargetView* const*>(m_pRenderTargetView.GetAddressOf()), nullptr);
	}

	void RenderManager::RenderOnlyRenderer()
	{
		// Init Queue 처리
		while (!m_initQueue.empty()) {
			auto& renderer = m_initQueue.front();

			renderer->Initialize();
			m_initQueue.pop();
		}

		// 캠 버퍼 업데이트
		m_camMat.camPos = XMMatrixInverse(nullptr, m_viewMatrix).r[3];
		m_camMat.mView = m_viewMatrix;
		m_camMat.mProjection = m_projMatrix;

		// 리소스 업데이트
		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);

		// 기본 렌더셋팅
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->PSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());

		m_pDeviceContext->RSSetState(m_pDefaultRS.Get());

		// RenderPass
		for (const auto& pass : m_Passes) {
			for (const auto& renderer : pass.second) {
				renderer->Render();
			}
		}
	}

	void RenderManager::EndFrame()
	{
		// Present our back buffer to our front buffer
		m_pSwapChain->Present(m_rSyncInterval, 0);
	}

	/*const int RenderManager::PropertyToIdx(const std::wstring& _propertyName) const
	{
		auto it = m_propertyMap.find(_propertyName);
		if (it == m_propertyMap.end())
			return -1;

		return it->second;
	}*/
}
