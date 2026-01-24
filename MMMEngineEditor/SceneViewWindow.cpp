#include "imgui.h"
#include "SceneViewWindow.h"
#include "EditorRegistry.h"
#include "RenderStateGuard.h"
#include "RenderManager.h"
#include <ImGuizmo.h>
#include "Transform.h"
#include <imgui_internal.h>

using namespace MMMEngine::Editor;
using namespace MMMEngine;
using namespace MMMEngine::Utility;
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
	wc.ParentViewportId = ImGui::GetMainViewport()->ID;
	wc.ViewportFlagsOverrideSet = ImGuiViewportFlags_NoFocusOnAppearing;
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

	auto scenecornerpos = ImGui::GetCursorPos();

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
	// ImGuizmo는 별도의 DrawList에 그려짐
	if (g_selectedGameObject.IsValid())
	{
		ImVec2 imagePos = ImGui::GetItemRectMin();  // 방금 그린 Image의 좌상단 (화면 좌표)
		ImVec2 imageMax = ImGui::GetItemRectMax();
		ImVec2 imageSize = ImVec2(imageMax.x - imagePos.x, imageMax.y - imagePos.y);

		ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);

		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
		ImGuizmo::SetOrthographic(false);

		auto viewMat = m_pCam->GetViewMatrix();
		auto projMat = m_pCam->GetProjMatrix();
		auto modelMat = g_selectedGameObject->GetTransform()->GetWorldMatrix(); // 값이라도 로컬에 저장

		float* viewPtr = &viewMat.m[0][0];
		float* projPtr = &projMat.m[0][0];
		float* modelPtr = &modelMat.m[0][0];

		ImGuizmo::Manipulate(viewPtr, projPtr, m_guizmoOperation, m_guizmoMode, modelPtr);

		if (ImGuizmo::IsUsing())
		{
			Vector3 t, s;
			Quaternion r;
			modelMat.Decompose(s, r, t);

			auto tr = g_selectedGameObject->GetTransform();

			// 회전 중에는 scale 드리프트를 막기 위해 기존 스케일 유지
			if (m_guizmoOperation == ImGuizmo::ROTATE)
				s = tr->GetWorldScale();

			r.Normalize(); // 쿼터니언도 정규화 추천

			tr->SetWorldPosition(t);
			tr->SetWorldRotation(r);
			tr->SetWorldScale(s);
		}
	}

	{
		auto buttonsize = ImVec2(0, 0);
		auto padding = ImVec2{ 10,10 };
		auto moving = m_guizmoOperation == ImGuizmo::OPERATION::TRANSLATE;
		auto rotating = m_guizmoOperation == ImGuizmo::OPERATION::ROTATE;
		auto scaling = m_guizmoOperation == ImGuizmo::OPERATION::SCALE;
		auto local = m_guizmoMode == ImGuizmo::MODE::LOCAL;
		auto world = m_guizmoMode == ImGuizmo::MODE::WORLD;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));

		ImGui::SetCursorPos(scenecornerpos + padding);
		// Move 버튼
		ImGui::BeginDisabled(moving);
		if (ImGui::Button(u8"\uf047 move", buttonsize))
		{
			m_guizmoOperation = ImGuizmo::TRANSLATE;
		}
		ImGui::EndDisabled();

		ImGui::SameLine();

		// Rotate 버튼
		ImGui::BeginDisabled(rotating);
		if (ImGui::Button(u8"\uf2f1 rotate", buttonsize))
		{
			m_guizmoOperation = ImGuizmo::ROTATE;
		}
		ImGui::EndDisabled();

		ImGui::SameLine();

		// Scale 버튼
		ImGui::BeginDisabled(scaling);
		if (ImGui::Button(u8"\uf31e scale", buttonsize))
		{
			m_guizmoOperation = ImGuizmo::SCALE;
		}
		ImGui::EndDisabled();

		ImGui::SameLine();

		// 구분선
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);

		ImGui::SameLine();

		// Local 버튼
		ImGui::BeginDisabled(local);
		if (ImGui::Button(u8"\uf1b2 local", buttonsize))
		{
			m_guizmoMode = ImGuizmo::LOCAL;
		}
		ImGui::EndDisabled();

		ImGui::SameLine();

		// World 버튼
		ImGui::BeginDisabled(world);
		if (ImGui::Button(u8"\uf0ac world", buttonsize))
		{
			m_guizmoMode = ImGuizmo::WORLD;
		}
		ImGui::EndDisabled();

		ImGui::PopStyleColor(3);
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

	RenderStateGuard guard(context); // 백업/복원만 담당

	ID3D11RenderTargetView* rtv = m_pSceneRTV.Get();
	ID3D11DepthStencilView* dsv = m_pSceneDSV.Get();
	context->OMSetRenderTargets(1, &rtv, dsv);

	// Viewport
	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(m_width);
	viewport.Height = static_cast<float>(m_height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	context->RSSetViewports(1, &viewport);

	// Clear (RTV/DSV가 바인딩된 뒤에 하는 게 안전)
	float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	context->ClearRenderTargetView(rtv, clearColor);
	context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	if (m_isFocused)
		m_pCam->InputUpdate();

	m_pGridRenderer->Render(context, *m_pCam);

	auto view = m_pCam->GetViewMatrix();
	auto proj = m_pCam->GetProjMatrix();

	RenderManager::Get().SetViewMatrix(view);
	RenderManager::Get().SetProjMatrix(proj);
	RenderManager::Get().RenderOnlyRenderer();

	// 여기서 함수 끝나면 guard 소멸자에서 원래 RT/Viewport/Blend 등 자동 복원됨
}
