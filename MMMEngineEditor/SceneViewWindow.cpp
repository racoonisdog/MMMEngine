#include "imgui.h"
#include "SceneViewWindow.h"
#include "EditorRegistry.h"

using namespace MMMEngine::Editor;
using namespace MMMEngine;
using namespace MMMEngine::EditorRegistry;



void MMMEngine::Editor::SceneViewWindow::Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int initWidth, int initHeight)
{
	m_cachedDevice = device;
	m_cachedContext = context;

	m_width = initWidth;
	m_height = initHeight;
	m_lastWidth = initWidth;
	m_lastHeight = initHeight;

	if (!m_pCam)
	{
		m_pCam = std::make_unique<EditorCamera>();
		m_pCam->SetPosition(0.0f, 5.0f, -10.0f);
		m_pCam->SetEulerRotation(DirectX::SimpleMath::Vector3(15.0f, 0.0f, 0.0f));
		m_pCam->SetFOV(60.0f);
		m_pCam->SetNearPlane(0.1f);
		m_pCam->SetFarPlane(1000.0f);
		m_pCam->SetAspectRatio((float)initWidth, (float)initHeight);
	}

	m_pGridRenderer = std::make_unique<EditorGridRenderer>();
	if (!m_pGridRenderer->Initialize(device))
	{
		// 에러 처리
		OutputDebugStringA("Failed to initialize Grid Renderer\n");
	}

	CreateRenderTargets(device, m_lastWidth, m_lastHeight);
}

void MMMEngine::Editor::SceneViewWindow::Render()
{
	if (!g_editor_window_sceneView)
		return;

	ResizeRenderTarget(m_cachedDevice, m_lastWidth, m_lastHeight);

	RenderSceneToTexture(m_cachedContext);

	ImGuiWindowClass wc;
	// 핵심: 메인 뷰포트에 이 윈도우를 종속시킵니다.
	// 이렇게 하면 메인 창을 클릭해도 이 창이 '메인 창의 일부'로서 취급되어 우선순위를 가집니다.
	wc.ParentViewportId = ImGui::GetMainViewport()->ID;
	wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing; // 필요 시 설정

	ImGui::SetNextWindowClass(&wc);

	ImGuiStyle& style = ImGui::GetStyle();
	style.WindowMenuButtonPosition = ImGuiDir_None;

	ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
	ImGui::SetNextWindowSize(ImVec2(m_width, m_height), ImGuiCond_FirstUseEver);

	ImGui::Begin(u8"\uf009 씬", &g_editor_window_sceneView);

	if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right))
	{
		ImGui::SetWindowFocus();
	}

	m_isHovered = ImGui::IsWindowHovered();
	m_isFocused = ImGui::IsWindowFocused();

	// 사용 가능한 영역 크기 가져오기
	ImVec2 viewportSize = ImGui::GetContentRegionAvail();

	m_lastWidth = viewportSize.x;
	m_lastHeight = viewportSize.y;

	// 크기가 변경되었으면 렌더 타겟 리사이즈 필요
	// 실제 리사이즈는 다음 프레임의 Render에서 수행

	// ImGui에 텍스처 렌더링
	if (m_pSceneSRV)
	{
		ImGui::Image(
			(ImTextureID)m_pSceneSRV.Get(),
			viewportSize,
			ImVec2(0, 0),
			ImVec2(1, 1)
		);
	}


	ImGui::End();
	ImGui::PopStyleVar();
}

bool MMMEngine::Editor::SceneViewWindow::CreateRenderTargets(ID3D11Device* device, int width, int height)
{
	// 기존 리소스 해제
	m_pSceneTexture.Reset();
	m_pSceneRTV.Reset();
	m_pSceneSRV.Reset();
	m_pSceneDSV.Reset();
	m_pDepthStencilBuffer.Reset();

	m_width = width;
	m_height = height;

	// Render Target Texture 생성
	D3D11_TEXTURE2D_DESC textureDesc = {};
	textureDesc.Width = width;
	textureDesc.Height = height;
	textureDesc.MipLevels = 1;
	textureDesc.ArraySize = 1;
	textureDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	textureDesc.SampleDesc.Count = 1;
	textureDesc.SampleDesc.Quality = 0;
	textureDesc.Usage = D3D11_USAGE_DEFAULT;
	textureDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
	textureDesc.CPUAccessFlags = 0;
	textureDesc.MiscFlags = 0;

	HRESULT hr = device->CreateTexture2D(&textureDesc, nullptr, m_pSceneTexture.GetAddressOf());
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create scene texture\n");
		return false;
	}

	// Render Target View 생성
	D3D11_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = textureDesc.Format;
	rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;

	hr = device->CreateRenderTargetView(m_pSceneTexture.Get(), &rtvDesc, m_pSceneRTV.GetAddressOf());
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create RTV\n");
		return false;
	}

	// Shader Resource View 생성 (ImGui에서 사용)
	D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = textureDesc.Format;
	srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;

	hr = device->CreateShaderResourceView(m_pSceneTexture.Get(), &srvDesc, m_pSceneSRV.GetAddressOf());
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create SRV\n");
		return false;
	}

	// Depth Stencil Buffer 생성
	D3D11_TEXTURE2D_DESC depthDesc = {};
	depthDesc.Width = width;
	depthDesc.Height = height;
	depthDesc.MipLevels = 1;
	depthDesc.ArraySize = 1;
	depthDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	depthDesc.SampleDesc.Count = 1;
	depthDesc.SampleDesc.Quality = 0;
	depthDesc.Usage = D3D11_USAGE_DEFAULT;
	depthDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
	depthDesc.CPUAccessFlags = 0;
	depthDesc.MiscFlags = 0;

	hr = device->CreateTexture2D(&depthDesc, nullptr, m_pDepthStencilBuffer.GetAddressOf());
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create depth stencil buffer\n");
		return false;
	}

	// Depth Stencil View 생성
	D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
	dsvDesc.Format = depthDesc.Format;
	dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;

	hr = device->CreateDepthStencilView(m_pDepthStencilBuffer.Get(), &dsvDesc, m_pSceneDSV.GetAddressOf());
	if (FAILED(hr))
	{
		OutputDebugStringA("Failed to create DSV\n");
		return false;
	}

	// 카메라 aspect ratio 업데이트
	m_pCam->SetAspectRatio((float)width, (float)height);

	return true;
}

void MMMEngine::Editor::SceneViewWindow::ResizeRenderTarget(ID3D11Device* device, int width, int height)
{
	if (width > 0 && height > 0 && (width != m_width || height != m_height))
	{
		CreateRenderTargets(device, width, height);
	}
}

void MMMEngine::Editor::SceneViewWindow::RenderSceneToTexture(ID3D11DeviceContext* context)
{
	ID3D11ShaderResourceView* nullSRV = nullptr;
	context->PSSetShaderResources(0, 1, &nullSRV);

	// Render Target과 Depth Stencil 설정
	ID3D11RenderTargetView* oldRTV = nullptr;
	ID3D11DepthStencilView* oldDSV = nullptr;
	context->OMGetRenderTargets(1, &oldRTV, &oldDSV);
	UINT numViewports = 1;
	D3D11_VIEWPORT oldViewport;
	context->RSGetViewports(&numViewports, &oldViewport);

	context->OMSetRenderTargets(1, m_pSceneRTV.GetAddressOf(), m_pSceneDSV.Get());


	// Viewport 설정
	D3D11_VIEWPORT viewport = {};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(m_width);
	viewport.Height = static_cast<float>(m_height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	context->RSSetViewports(1, &viewport);

	// Clear
	float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	context->ClearRenderTargetView(m_pSceneRTV.Get(), clearColor);
	context->ClearDepthStencilView(m_pSceneDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	// 카메라 입력 업데이트 (윈도우가 focused일 때만)
	if (m_isFocused)
	{
		m_pCam->InputUpdate();
	}

	// 그리드 렌더링
	m_pGridRenderer->Render(context, *m_pCam);

	// TODO: 여기에 다른 씬 오브젝트들 렌더링

	// 3. 원래 상태로 복구
	context->OMSetRenderTargets(1, &oldRTV, oldDSV);
	context->RSSetViewports(1, &oldViewport);

	// 4. OMGet으로 올라간 참조 카운트 해제 (메모리 누수 방지)
	if (oldRTV) oldRTV->Release();
	if (oldDSV) oldDSV->Release();
}
