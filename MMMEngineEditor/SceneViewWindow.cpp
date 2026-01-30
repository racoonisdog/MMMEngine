#include "imgui.h"
#include "SceneViewWindow.h"
#include "EditorRegistry.h"
#include "RenderStateGuard.h"
#include "RenderManager.h"
#include "SceneManager.h"
#include <ImGuizmo.h>
#include "Transform.h"
#include <imgui_internal.h>
#include "ColliderComponent.h"
#include "Camera.h"
#include "RigidBodyComponent.h"
#include <memory>

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
		m_pCam->SetPosition(0.0f, 5.0f, 10.0f);
		m_pCam->SetEulerRotation(DirectX::SimpleMath::Vector3(15.0f, 180.0f, 0.0f));
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

	m_states = std::make_unique<CommonStates>(m_cachedDevice);
	m_batch = std::make_unique<PrimitiveBatch<VertexPositionColor>>(m_cachedContext);
	m_effect = std::make_unique<BasicEffect>(m_cachedDevice);
	m_effect->SetVertexColorEnabled(true);
	{
		void const* shaderByteCode;
		size_t byteCodeLength;

		m_effect->GetVertexShaderBytecode(&shaderByteCode, &byteCodeLength);

		m_cachedDevice->CreateInputLayout(
			VertexPositionColor::InputElements, VertexPositionColor::InputElementCount,
			shaderByteCode, byteCodeLength,
			m_pDebugDrawIL.ReleaseAndGetAddressOf());
	}
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

	if (m_isFocused && m_isHovered)
	{
		ImGuiIO& io = ImGui::GetIO();
		const bool rightDown = ImGui::IsMouseDown(ImGuiMouseButton_Right);

		// 수정 제안: WantCaptureKeyboard 조건을 제거하거나 
		// ImGui Key 관련 함수를 직접 사용하여 우선순위를 높입니다.
		const bool gizmoUsing = ImGuizmo::IsUsing();

		if (!rightDown && !gizmoUsing)
		{
			if (ImGui::IsKeyPressed(ImGuiKey_Q))
				m_guizmoOperation = (ImGuizmo::OPERATION)0; // 0은 어떤 기즈모도 표시하지 않음

			if (ImGui::IsKeyPressed(ImGuiKey_W))
				m_guizmoOperation = ImGuizmo::TRANSLATE;

			if (ImGui::IsKeyPressed(ImGuiKey_E))
				m_guizmoOperation = ImGuizmo::ROTATE;

			if (ImGui::IsKeyPressed(ImGuiKey_R))
				m_guizmoOperation = ImGuizmo::SCALE;
		}

		if (ImGui::IsKeyPressed(ImGuiKey_F))
		{
			if (g_selectedGameObject.IsValid())
			{
				auto& tr = g_selectedGameObject->GetTransform();
				// 오브젝트의 위치로 포커스 (거리는 5.0f로 설정하거나 바운딩 박스 크기에 비례하게 설정)
				m_pCam->FocusOn(tr->GetWorldPosition(), 7.0f);
			}
		}
	}


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
	if (g_selectedGameObject.IsValid() && (int)m_guizmoOperation != 0)
	{
		ImVec2 imagePos = ImGui::GetItemRectMin();  // 방금 그린 Image의 좌상단 (화면 좌표)
		ImVec2 imageMax = ImGui::GetItemRectMax();
		ImVec2 imageSize = ImVec2(imageMax.x - imagePos.x, imageMax.y - imagePos.y);

		ImGuizmo::SetRect(imagePos.x, imagePos.y, imageSize.x, imageSize.y);

		float snapValue[3] = { 0.f, 0.f, 0.f };
		bool useSnap = ImGui::GetIO().KeyCtrl;

		if (useSnap)
		{
			if (m_guizmoOperation == ImGuizmo::TRANSLATE)
				snapValue[0] = snapValue[1] = snapValue[2] = 0.5f; // 0.5 단위 이동
			else if (m_guizmoOperation == ImGuizmo::ROTATE)
				snapValue[0] = snapValue[1] = snapValue[2] = 15.0f; // 15도 단위 회전
			else if (m_guizmoOperation == ImGuizmo::SCALE)
				snapValue[0] = snapValue[1] = snapValue[2] = 0.1f; // 0.1 단위 스케일
		}

		ImGuizmo::SetDrawlist(ImGui::GetWindowDrawList());
		ImGuizmo::SetOrthographic(false);

		auto viewMat = m_pCam->GetViewMatrix();
		auto projMat = m_pCam->GetProjMatrix();
		
		auto modelMat = Matrix::Identity;
			
		if(g_selectedGameObject.IsValid() && !g_selectedGameObject->IsDestroyed()
			&& g_selectedGameObject->GetTransform().IsValid() && !g_selectedGameObject->GetTransform()->IsDestroyed())
			modelMat = g_selectedGameObject->GetTransform()->GetWorldMatrix(); // 값이라도 로컬에 저장

		float* viewPtr = &viewMat.m[0][0];
		float* projPtr = &projMat.m[0][0];
		float* modelPtr = &modelMat.m[0][0];

		ImGuizmo::Manipulate(viewPtr, projPtr, m_guizmoOperation, m_guizmoMode, modelPtr, NULL, useSnap ? snapValue : NULL);

		if (ImGuizmo::IsUsing())
		{
			Vector3 t, s;
			Quaternion r;
			modelMat.Decompose(s, r, t);

			auto tr = g_selectedGameObject->GetTransform();

			// 3. SnapToZero 적용 (미세한 오차 제거)
			auto SnapToZero = [](float& v, float eps = 1e-4f) {
				if (std::abs(v) < eps) v = 0.0f;
				};

			if (m_guizmoOperation == ImGuizmo::ROTATE)
			{
				s = tr->GetWorldScale();
			}
			else
			{
				SnapToZero(s.x); SnapToZero(s.y); SnapToZero(s.z);
			}

			SnapToZero(t.x); SnapToZero(t.y); SnapToZero(t.z);

			r.Normalize();

			tr->SetWorldPosition(t);
			tr->SetWorldRotation(r);
			tr->SetWorldScale(s);

			if (g_editor_scene_playing)
			{
				auto rbPtr = g_selectedGameObject->GetComponent<RigidBodyComponent>();
				if (rbPtr.IsValid())
				{
					if (rbPtr->GetKinematic())
						rbPtr->SetKinematicTarget(t, r);
					else
						rbPtr->Editor_changeTrans(t, r);
				}
			}
		}
	}

	{
		auto buttonsize = ImVec2(0, 0);
		auto padding = ImVec2{ 10,10 };
		auto handing = (int)m_guizmoOperation == 0;
		auto moving = m_guizmoOperation == ImGuizmo::OPERATION::TRANSLATE;
		auto rotating = m_guizmoOperation == ImGuizmo::OPERATION::ROTATE;
		auto scaling = m_guizmoOperation == ImGuizmo::OPERATION::SCALE;
		auto local = m_guizmoMode == ImGuizmo::MODE::LOCAL;
		auto world = m_guizmoMode == ImGuizmo::MODE::WORLD;

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));

		ImGui::SetCursorPos(scenecornerpos + padding);
		// Hand 버튼 (Q)
		ImGui::BeginDisabled(handing);
		if (ImGui::Button(u8"\uf256 hand", buttonsize)) // 폰트어썸 핸드 아이콘
		{
			m_guizmoOperation = (ImGuizmo::OPERATION)0;
		}
		ImGui::EndDisabled();
		ImGui::SameLine();
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

		// --- 카메라 설정 팝업 버튼 추가 ---
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		if (ImGui::Button(u8"\uf0ad Camera Settings")) // 폰트어썸 렌치(wrench) 아이콘 사용 예시
		{
			ImGui::OpenPopup("CameraSettingsPopup");
		}

		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

		// 팝업 창 정의
		if (ImGui::BeginPopup("CameraSettingsPopup"))
		{
			float fov = m_pCam->GetFOV();
			float n = m_pCam->GetNearPlane();
			float f = m_pCam->GetFarPlane();

			// 컨트롤 간의 간격을 위해 ItemSpacing도 조절하고 싶다면 추가 가능
			if (ImGui::DragFloat("FOV", &fov, 0.5f, 10.0f, 120.0f)) m_pCam->SetFOV(fov);
			if (ImGui::DragFloat("Near", &n, 0.01f, 0.01f, 10.0f)) m_pCam->SetNearPlane(n);
			if (ImGui::DragFloat("Far", &f, 1.0f, 10.0f, 10000.0f)) m_pCam->SetFarPlane(f);

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(2);
		// ----------------------------------

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


	D3D11_TEXTURE2D_DESC idDesc = {};
	idDesc.Width = width;
	idDesc.Height = height;
	idDesc.MipLevels = 1;
	idDesc.ArraySize = 1;
	idDesc.Format = DXGI_FORMAT_R32_UINT;           // 핵심
	idDesc.SampleDesc.Count = 1;
	idDesc.SampleDesc.Quality = 0;
	idDesc.Usage = D3D11_USAGE_DEFAULT;
	idDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	device->CreateTexture2D(&idDesc, nullptr, m_pIdTexture.GetAddressOf());
	device->CreateRenderTargetView(m_pIdTexture.Get(), nullptr, m_pIdRTV.GetAddressOf());
	device->CreateShaderResourceView(m_pIdTexture.Get(), nullptr, m_pIdSRV.GetAddressOf());

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
	ID3D11RenderTargetView* rtvs[2] = { m_pSceneRTV.Get(), m_pIdRTV.Get() };
	context->OMSetRenderTargets(2, rtvs, dsv);

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
		m_pCam->InputUpdate((int)m_guizmoOperation);

	m_pGridRenderer->Render(context, *m_pCam);

	auto view = m_pCam->GetViewMatrix();
	auto proj = m_pCam->GetProjMatrix();

	RenderManager::Get().SetViewMatrix(view);
	RenderManager::Get().SetProjMatrix(proj);
	RenderManager::Get().RenderOnlyRenderer();

	// 디버그 드로잉
	if (m_enableDebugDraw)
	{
		if (!m_enableDebugDrawZbuffer)
		{
			context->OMSetBlendState(m_states->Opaque(), nullptr, 0xFFFFFFFF);
			context->OMSetDepthStencilState(m_states->DepthNone(), 0);
			context->RSSetState(m_states->CullNone());
		}

		m_effect->SetView(view);
		m_effect->SetProjection(proj);
		m_effect->Apply(context);
		context->IASetInputLayout(m_pDebugDrawIL.Get());

		m_batch->Begin();

		auto& sceneGameObjects = SceneManager::Get().GetAllGameObjectInCurrentScene();

		for (size_t i = 0; i < sceneGameObjects.size(); ++i)
		{
			auto& go = sceneGameObjects[i];

			if (!go->IsActiveInHierarchy())
				continue;

			auto& ColliderComponents = go->GetComponents<ColliderComponent>();

			for (auto& col : ColliderComponents)
			{
				if (!col.IsValid() && col->IsDestroyed())
				{
					continue;
				}
				auto desc = col->GetDebugShapeDesc();

				switch (desc.type)
				{
				case ColliderComponent::DebugColliderType::Box:
				{
					BoundingBox box;
					box.Center = desc.localCenter;
					box.Extents = desc.halfExtents;
					BoundingOrientedBox obb;
					obb.CreateFromBoundingBox(obb, box);
					obb.Transform(obb, go->GetTransform()->GetWorldMatrix());
					DX::Draw(m_batch.get(), obb, Colors::LightGreen);
					break;
				}
				case ColliderComponent::DebugColliderType::Sphere:
				{
					BoundingSphere sphere;
					sphere.Center = desc.localCenter;
					sphere.Radius = desc.sphereRadius;
					DX::Draw(m_batch.get(), sphere, go->GetTransform()->GetWorldMatrix(), Colors::LightGreen);
					break;
				}
				case ColliderComponent::DebugColliderType::Capsule:
				{
					const auto& wm = go->GetTransform()->GetWorldMatrix();

					const Vector3 upV = wm.Up();
					const Vector3 rightV = wm.Right();
					const Vector3 forwardV = wm.Forward();

					const float r = desc.radius;

					const Vector3 worldPos = go->GetTransform()->GetWorldPosition();
					const Vector3 p0 = worldPos + upV * desc.halfHeight; // 상단 구 중심
					const Vector3 p1 = worldPos - upV * desc.halfHeight; // 하단 구 중심

					const XMVECTOR color = Colors::LightGreen;

					// 축 벡터를 XMVECTOR로 (ring/arc에 사용)
					const XMVECTOR rightAxis = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&rightV)) * r;
					const XMVECTOR forwardAxis = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&forwardV)) * r;
					const XMVECTOR upAxis = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&upV)) * r;

					// ---- A) 절단원 링 2개 (원기둥 위/아래 단면) ----
					{
						const XMVECTOR o0 = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&p0));
						const XMVECTOR o1 = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&p1));

						// Up에 수직인 평면: major=Right*r, minor=Forward*r
						DX::DrawRing(m_batch.get(), o0, rightAxis, forwardAxis, color);
						DX::DrawRing(m_batch.get(), o1, rightAxis, forwardAxis, color);
					}

					// ---- B) 원기둥(측면) 4개 선 ----
					{
						m_batch->DrawLine(
							VertexPositionColor(p0 + rightV * r, Colors::LightGreen),
							VertexPositionColor(p1 + rightV * r, Colors::LightGreen));

						m_batch->DrawLine(
							VertexPositionColor(p0 - rightV * r, Colors::LightGreen),
							VertexPositionColor(p1 - rightV * r, Colors::LightGreen));

						m_batch->DrawLine(
							VertexPositionColor(p0 + forwardV * r, Colors::LightGreen),
							VertexPositionColor(p1 + forwardV * r, Colors::LightGreen));

						m_batch->DrawLine(
							VertexPositionColor(p0 - forwardV * r, Colors::LightGreen),
							VertexPositionColor(p1 - forwardV * r, Colors::LightGreen));
					}

					// ---- C) 상단 반구: 하프링 2개 ----
					{
						const XMVECTOR o = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&p0));

						// sin>=0 쪽이 +Up이므로 "상단" 반원
						DX::DrawHalfRing(m_batch.get(), o, rightAxis, upAxis, color, 16);
						DX::DrawHalfRing(m_batch.get(), o, forwardAxis, upAxis, color, 16);
					}

					// ---- D) 하단 반구: 하프링 2개 ----
					{
						const XMVECTOR o = XMLoadFloat3(reinterpret_cast<const XMFLOAT3*>(&p1));

						// 하단은 minorAxis를 -Up으로 뒤집으면 됨
						const XMVECTOR downAxis = -upAxis;

						DX::DrawHalfRing(m_batch.get(), o, rightAxis, downAxis, color, 16);
						DX::DrawHalfRing(m_batch.get(), o, forwardAxis, downAxis, color, 16);
					}

					break;
				}
				default:
					break;
				}
			}
		}

		if (Camera::GetMainCamera().IsValid() &&
			g_selectedGameObject == Camera::GetMainCamera()->GetGameObject())
		{
			auto mainCam = Camera::GetMainCamera();

			BoundingFrustum frustum;
			BoundingFrustum::CreateFromMatrix(frustum, mainCam->GetProjMatrix());
			frustum.Transform(frustum, mainCam->GetTransform()->GetWorldMatrix());

			DX::Draw(m_batch.get(), frustum, Colors::GhostWhite);
		}

			

		m_batch->End();
	}

	// 여기서 함수 끝나면 guard 소멸자에서 원래 RT/Viewport/Blend 등 자동 복원됨
}
