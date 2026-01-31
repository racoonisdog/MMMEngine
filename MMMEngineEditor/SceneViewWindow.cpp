#include "imgui.h"
#include "SceneViewWindow.h"
#include "EditorRegistry.h"
#include "RenderStateGuard.h"
#include "RenderManager.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "VShader.h"
#include "PShader.h"
#include "SceneManager.h"
#include <ImGuizmo.h>
#include "Transform.h"
#include <imgui_internal.h>
#include "ColliderComponent.h"
#include "Camera.h"
#include "RigidBodyComponent.h"
#include <memory>
#include <algorithm>
#include <cmath>
#include <vector>

using namespace MMMEngine::Editor;
using namespace MMMEngine;
using namespace MMMEngine::Utility;
using namespace MMMEngine::EditorRegistry;

namespace
{
	struct PickingIdBuffer
	{
		uint32_t objectId = 0;
		uint32_t padding[3] = { 0, 0, 0 };
	};

	struct OutlineConstants
	{
		DirectX::SimpleMath::Vector4 color = { 1.0f, 0.5f, 0.0f, 1.0f };
		DirectX::SimpleMath::Vector2 texelSize = { 1.0f, 1.0f };
		DirectX::SimpleMath::Vector2 padding0 = { 0.0f, 0.0f };
		float thickness = 1.0f;
		float threshold = 0.2f;
		DirectX::SimpleMath::Vector2 padding1 = { 0.0f, 0.0f };
	};
}

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
		m_viewGizmoDistance = 10.0f;
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

	// Picking ID constant buffer
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(PickingIdBuffer);
		m_cachedDevice->CreateBuffer(&bd, nullptr, m_pPickingIdBuffer.GetAddressOf());
	}

	// Outline constant buffer
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.ByteWidth = sizeof(OutlineConstants);
		m_cachedDevice->CreateBuffer(&bd, nullptr, m_pOutlineCBuffer.GetAddressOf());
	}

	// Mask depth state (always pass, no write)
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = TRUE;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.StencilEnable = FALSE;
		m_cachedDevice->CreateDepthStencilState(&dsDesc, m_pMaskDepthState.GetAddressOf());
	}

	// No-color-write blend state (stencil-only pass)
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = FALSE;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = 0;
		m_cachedDevice->CreateBlendState(&blendDesc, m_pNoColorWriteBS.GetAddressOf());
	}

	// Stencil write state (always pass, replace)
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = TRUE;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.StencilEnable = TRUE;
		dsDesc.StencilReadMask = 0xFF;
		dsDesc.StencilWriteMask = 0xFF;
		dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_REPLACE;
		dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.BackFace = dsDesc.FrontFace;
		m_cachedDevice->CreateDepthStencilState(&dsDesc, m_pStencilWriteState.GetAddressOf());
	}

	// Stencil test state (equal)
	{
		D3D11_DEPTH_STENCIL_DESC dsDesc = {};
		dsDesc.DepthEnable = FALSE;
		dsDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		dsDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		dsDesc.StencilEnable = TRUE;
		dsDesc.StencilReadMask = 0xFF;
		dsDesc.StencilWriteMask = 0xFF;
		dsDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		dsDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		dsDesc.BackFace = dsDesc.FrontFace;
		m_cachedDevice->CreateDepthStencilState(&dsDesc, m_pStencilTestState.GetAddressOf());
	}

	// Outline blend state
	{
		D3D11_BLEND_DESC blendDesc = {};
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		// Preserve destination alpha so ImGui doesn't treat the scene as transparent.
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ZERO;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		m_cachedDevice->CreateBlendState(&blendDesc, m_pOutlineBlendState.GetAddressOf());
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
	ImGui::SetNextWindowSize(ImVec2(static_cast<float>(m_width), static_cast<float>(m_height)), ImGuiCond_FirstUseEver);

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
				const float focusDistance = 7.0f;
				m_pCam->FocusOn(tr->GetWorldPosition(), focusDistance);
				m_viewGizmoDistance = focusDistance;
			}
		}
	}


	// 사용 가능한 영역 크기 가져오기
	ImVec2 viewportSize = ImGui::GetContentRegionAvail();
	m_lastWidth = static_cast<int>(viewportSize.x);
	m_lastHeight = static_cast<int>(viewportSize.y);

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

	ImVec2 imagePos = ImGui::GetItemRectMin();
	ImVec2 imageMax = ImGui::GetItemRectMax();
	ImVec2 imageSize = ImVec2(imageMax.x - imagePos.x, imageMax.y - imagePos.y);
	bool gizmoDrawn = false;
	// ImGuizmo는 별도의 DrawList에 그려짐
	if (g_selectedGameObject.IsValid() && (int)m_guizmoOperation != 0)
	{
		gizmoDrawn = true;

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
		ImGuizmo::SetOrthographic(m_pCam->IsOrthographic());

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

	bool toolbuttonHovered = false;
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
		if(ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::EndDisabled();
		ImGui::SameLine();
		// Move 버튼
		ImGui::BeginDisabled(moving);
		if (ImGui::Button(u8"\uf047 move", buttonsize))
		{
			m_guizmoOperation = ImGuizmo::TRANSLATE;
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::EndDisabled();

		ImGui::SameLine();

		// Rotate 버튼
		ImGui::BeginDisabled(rotating);
		if (ImGui::Button(u8"\uf2f1 rotate", buttonsize))
		{
			m_guizmoOperation = ImGuizmo::ROTATE;
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::EndDisabled();

		ImGui::SameLine();

		// Scale 버튼
		ImGui::BeginDisabled(scaling);
		if (ImGui::Button(u8"\uf31e scale", buttonsize))
		{
			m_guizmoOperation = ImGuizmo::SCALE;
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
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
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::EndDisabled();

		ImGui::SameLine();

		// World 버튼
		ImGui::BeginDisabled(world);
		if (ImGui::Button(u8"\uf0ac world", buttonsize))
		{
			m_guizmoMode = ImGuizmo::WORLD;
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::EndDisabled();

		// --- 카메라 설정 팝업 버튼 추가 ---
		ImGui::SameLine();
		ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
		ImGui::SameLine();

		if (ImGui::Button(u8"\uf0ad Camera Settings")) // 폰트어썸 렌치(wrench) 아이콘 사용 예시
		{
			ImGui::OpenPopup("CameraSettingsPopup");
		}
		if (ImGui::IsItemHovered())
			toolbuttonHovered = true;
		ImGui::PushStyleVar(ImGuiStyleVar_PopupRounding, 10.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8.0f, 8.0f));

		// 팝업 창 정의
		if (ImGui::BeginPopup("CameraSettingsPopup"))
		{
			float fov = m_pCam->GetFOV();
			float n = m_pCam->GetNearPlane();
			float f = m_pCam->GetFarPlane();
			bool ortho = m_pCam->IsOrthographicTarget();

			// 컨트롤 간의 간격을 위해 ItemSpacing도 조절하고 싶다면 추가 가능
			if (ImGui::Checkbox("Orthographic", &ortho)) m_pCam->SetOrthographic(ortho);
			if (!ortho)
			{
				if (ImGui::DragFloat("FOV", &fov, 0.5f, 10.0f, 120.0f)) m_pCam->SetFOV(fov);
			}
			else
			{
				float orthoSize = m_pCam->GetOrthoSize();
				if (ImGui::DragFloat("Ortho Size", &orthoSize, 0.1f, 0.1f, 1000.0f)) m_pCam->SetOrthoSize(orthoSize);
			}
			if (ImGui::DragFloat("Near", &n, 0.01f, 0.01f, 10.0f)) m_pCam->SetNearPlane(n);
			if (ImGui::DragFloat("Far", &f, 1.0f, 10.0f, 10000.0f)) m_pCam->SetFarPlane(f);

			ImGui::EndPopup();
		}
		ImGui::PopStyleVar(2);
		// ----------------------------------

		ImGui::PopStyleColor(3);
	}

	bool viewGizmoUsing = false;
	if (imageSize.x > 0.0f && imageSize.y > 0.0f)
	{
		const float gizmoSize = 64.0f;
		const float gizmoPadding = 8.0f;
		const float axisLength = gizmoSize * 0.35f;
		const float circleRadius = gizmoSize * 0.12f;
		const float centerRadius = gizmoSize * 0.08f;

		ImVec2 gizmoCenter = ImVec2(
			imageMax.x - gizmoPadding - gizmoSize * 0.5f,
			imagePos.y + gizmoPadding + gizmoSize * 0.5f);
		ImVec2 gizmoMin = ImVec2(gizmoCenter.x - gizmoSize * 0.5f, gizmoCenter.y - gizmoSize * 0.5f);
		ImVec2 gizmoMax = ImVec2(gizmoCenter.x + gizmoSize * 0.5f, gizmoCenter.y + gizmoSize * 0.5f);
		const ImVec2 centerHalfSize = ImVec2(centerRadius * 0.75f, centerRadius * 0.75f);
		const ImVec2 centerMin = ImVec2(gizmoCenter.x - centerHalfSize.x, gizmoCenter.y - centerHalfSize.y);
		const ImVec2 centerMax = ImVec2(gizmoCenter.x + centerHalfSize.x, gizmoCenter.y + centerHalfSize.y);

		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(gizmoMin, gizmoMax, IM_COL32(20, 20, 20, 100), 6.0f);
		Matrix camWorld = m_pCam->GetTransformMatrix();
		Vector3 camRight = camWorld.Right();
		Vector3 camUp = camWorld.Up();
		Vector3 camForward = camWorld.Forward();

		struct AxisWidget
		{
			int index = 0;
			ImVec2 dir2D = {};
			float depth = 0.0f;
			ImU32 color = 0;
			const char* label = "";
			ImVec2 endPos = {};
			Vector3 axisWorld = Vector3::Zero;
			bool filled = true;
			float lineLength = 0.0f;
		};

		const ImU32 axisColors[3] =
		{
			IM_COL32(230, 70, 70, 255),
			IM_COL32(110, 230, 110, 255),
			IM_COL32(80, 140, 230, 255)
		};
		const char* axisLabels[3] = { "X", "Y", "Z" };

		AxisWidget axes[6];
		int axisCount = 0;
		for (int i = 0; i < 3; ++i)
		{
			Vector3 baseAxis = Vector3::Zero;
			if (i == 0) baseAxis = Vector3(1.0f, 0.0f, 0.0f);
			else if (i == 1) baseAxis = Vector3(0.0f, 1.0f, 0.0f);
			else baseAxis = Vector3(0.0f, 0.0f, 1.0f);

			for (int sign = 0; sign < 2; ++sign)
			{
				const float s = sign == 0 ? 1.0f : -1.0f;
				Vector3 axisWorld = baseAxis * s;

				Vector3 axisView = Vector3(
					axisWorld.Dot(camRight),
					axisWorld.Dot(camUp),
					axisWorld.Dot(camForward));
				ImVec2 dir2 = ImVec2(axisView.x, -axisView.y);
				float planar = std::sqrt(dir2.x * dir2.x + dir2.y * dir2.y);
				if (planar > 1e-5f)
				{
					dir2.x /= planar;
					dir2.y /= planar;
				}
				else
				{
					dir2.x = 0.0f;
					dir2.y = 0.0f;
				}

				float depthT = (axisView.z + 1.0f) * 0.5f;
				depthT = std::clamp(depthT, 0.0f, 1.0f);
				float length = axisLength * planar * (0.6f + 0.4f * depthT);

				AxisWidget& axis = axes[axisCount++];
				axis.index = i;
				axis.dir2D = dir2;
				axis.depth = axisView.z;
				axis.color = axisColors[i];
				axis.label = sign == 0 ? axisLabels[i] : "";
				axis.endPos = ImVec2(gizmoCenter.x + dir2.x * length, gizmoCenter.y + dir2.y * length);
				axis.axisWorld = axisWorld;
				axis.filled = (sign == 0);
				axis.lineLength = length;
			}
		}

		auto drawAxisLine = [&](const AxisWidget& axis)
		{
			float alpha = axis.depth >= 0.0f ? 1.0f : 0.55f;
			float lineAlpha = axis.filled ? 200.0f : 120.0f;
			ImU32 lineColor = IM_COL32(
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).x * 255.0f),
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).y * 255.0f),
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).z * 255.0f),
				(int)(alpha * lineAlpha));

			float lineLen = axis.lineLength - circleRadius;
			if (lineLen > 0.0f)
			{
				ImVec2 lineEnd = ImVec2(
					gizmoCenter.x + axis.dir2D.x * lineLen,
					gizmoCenter.y + axis.dir2D.y * lineLen);
				drawList->AddLine(gizmoCenter, lineEnd, lineColor, 2.0f);
			}
		};

		auto drawAxisCircleAndLabel = [&](const AxisWidget& axis)
		{
			float alpha = axis.depth >= 0.0f ? 1.0f : 0.55f;
			ImU32 circleColor = IM_COL32(
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).x * 255.0f),
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).y * 255.0f),
				(int)(ImGui::ColorConvertU32ToFloat4(axis.color).z * 255.0f),
				(int)(alpha * 255.0f));
			if (axis.filled)
			{
				drawList->AddCircleFilled(axis.endPos, circleRadius, circleColor);
			}
			else
			{
				drawList->AddCircle(axis.endPos, circleRadius, circleColor, 0, 2.0f);
			}

			if (axis.label && axis.label[0] != '\0')
			{
				ImVec2 textSize = ImGui::CalcTextSize(axis.label);
				drawList->AddText(ImVec2(axis.endPos.x - textSize.x * 0.5f, axis.endPos.y - textSize.y * 0.5f),
					IM_COL32(10, 10, 10, 220), axis.label);
			}
		};

		AxisWidget ordered[6];
		for (int i = 0; i < axisCount; ++i)
			ordered[i] = axes[i];
		std::sort(ordered, ordered + axisCount, [](const AxisWidget& a, const AxisWidget& b)
		{
			return a.depth < b.depth;
		});

		// Lines always behind center toggle
		for (int i = 0; i < axisCount; ++i)
			drawAxisLine(ordered[i]);

		// Back axes (faded) circles/labels behind center toggle
		for (int i = 0; i < axisCount; ++i)
		{
			if (ordered[i].depth < 0.0f)
				drawAxisCircleAndLabel(ordered[i]);
		}

		// center toggle (rounded rect) drawn after back axes so it stays visible
		drawList->AddRectFilled(centerMin, centerMax, IM_COL32(245, 245, 245, 240), 2.5f);

		// Front axes circles/labels above center toggle
		for (int i = 0; i < axisCount; ++i)
		{
			if (ordered[i].depth >= 0.0f)
				drawAxisCircleAndLabel(ordered[i]);
		}

		ImVec2 mousePos = ImGui::GetMousePos();
		bool anyHovered = false;
		int hoveredAxis = -1;
		for (int i = 0; i < axisCount; ++i)
		{
			ImVec2 delta = ImVec2(mousePos.x - axes[i].endPos.x, mousePos.y - axes[i].endPos.y);
			if (delta.x * delta.x + delta.y * delta.y <= circleRadius * circleRadius)
			{
				hoveredAxis = i;
				anyHovered = true;
				break;
			}
		}

		bool centerHovered = mousePos.x >= centerMin.x && mousePos.x <= centerMax.x
			&& mousePos.y >= centerMin.y && mousePos.y <= centerMax.y;
		if (centerHovered)
			anyHovered = true;

		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			if (centerHovered)
			{
				m_pCam->ToggleProjectionMode();
			}
			else if (hoveredAxis != -1)
			{
				Vector3 axisDir = axes[hoveredAxis].axisWorld;

				Matrix camWorld = m_pCam->GetTransformMatrix();
				Vector3 target = m_pCam->GetPosition() + camWorld.Forward() * (m_viewGizmoDistance < 0.1f ? 0.1f : m_viewGizmoDistance);

				const float viewDistance = m_viewGizmoDistance < 0.1f ? 0.1f : m_viewGizmoDistance;
				Vector3 eye = target + axisDir * viewDistance;

				Vector3 up = Vector3(0.0f, 1.0f, 0.0f);
				if (std::abs(axisDir.y) > 0.9f)
				{
					up = Vector3(0.0f, 0.0f, 1.0f);
				}

				DirectX::XMVECTOR eyeV = DirectX::XMVectorSet(eye.x, eye.y, eye.z, 1.0f);
				DirectX::XMVECTOR targetV = DirectX::XMVectorSet(target.x, target.y, target.z, 1.0f);
				DirectX::XMVECTOR upV = DirectX::XMVectorSet(up.x, up.y, up.z, 0.0f);

				Matrix newView;
				DirectX::XMStoreFloat4x4(&newView, DirectX::XMMatrixLookAtLH(eyeV, targetV, upV));
				Matrix invView = newView.Invert();
				m_pCam->SetPosition(invView.Translation());
				m_pCam->SetRotation(Quaternion::CreateFromRotationMatrix(invView));
				m_pCam->SyncInputState();
			}
		}

		viewGizmoUsing = anyHovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
		toolbuttonHovered = toolbuttonHovered || anyHovered || ImGui::IsMouseHoveringRect(gizmoMin, gizmoMax);
	}

	// 씬 뷰 픽킹 (좌클릭)
	{
		const bool gizmoBlocking = (gizmoDrawn && (ImGuizmo::IsOver() || ImGuizmo::IsUsing())) || viewGizmoUsing;
		ImVec2 mousePos = ImGui::GetMousePos();
		bool mouseInImage = mousePos.x >= imagePos.x && mousePos.x <= imageMax.x
			&& mousePos.y >= imagePos.y && mousePos.y <= imageMax.y;
		if (m_isHovered && mouseInImage && ImGui::IsMouseClicked(ImGuiMouseButton_Left)
			&& !gizmoBlocking
			&& !toolbuttonHovered)
		{
			ImVec2 imageSize = ImVec2(imageMax.x - imagePos.x, imageMax.y - imagePos.y);
			if (imageSize.x > 0.0f && imageSize.y > 0.0f)
			{
				float u = (mousePos.x - imagePos.x) / imageSize.x;
				float v = (mousePos.y - imagePos.y) / imageSize.y;
				int x = static_cast<int>(u * static_cast<float>(m_width));
				int y = static_cast<int>(v * static_cast<float>(m_height));

				if (x >= 0 && y >= 0 && x < m_width && y < m_height && m_pIdStagingTex)
				{
					m_cachedContext->CopyResource(m_pIdStagingTex.Get(), m_pIdTexture.Get());

					D3D11_MAPPED_SUBRESOURCE mapped = {};
					if (SUCCEEDED(m_cachedContext->Map(m_pIdStagingTex.Get(), 0, D3D11_MAP_READ, 0, &mapped)))
					{
						auto* row = reinterpret_cast<uint32_t*>(static_cast<uint8_t*>(mapped.pData) + y * mapped.RowPitch);
						uint32_t pickedId = row[x];
						m_cachedContext->Unmap(m_pIdStagingTex.Get(), 0);

						if (pickedId == 0)
						{
							g_selectedGameObject = nullptr;
						}
						else
						{
							auto* renderer = RenderManager::Get().GetRendererById(pickedId - 1);
							if (renderer && renderer->GetGameObject().IsValid() && !renderer->GetGameObject()->IsDestroyed())
							{
								g_selectedGameObject = renderer->GetGameObject();
							}
							else
							{
								g_selectedGameObject = nullptr;
							}
						}
					}
				}
			}
		}
	}
	m_blockCameraInput = viewGizmoUsing;
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
	m_pIdTexture.Reset();
	m_pIdRTV.Reset();
	m_pIdSRV.Reset();
	m_pIdStagingTex.Reset();
	m_pMaskTexture.Reset();
	m_pMaskRTV.Reset();
	m_pMaskSRV.Reset();

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

	D3D11_TEXTURE2D_DESC stagingDesc = idDesc;
	stagingDesc.Usage = D3D11_USAGE_STAGING;
	stagingDesc.BindFlags = 0;
	stagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
	stagingDesc.MiscFlags = 0;
	device->CreateTexture2D(&stagingDesc, nullptr, m_pIdStagingTex.GetAddressOf());

	D3D11_TEXTURE2D_DESC maskDesc = {};
	maskDesc.Width = width;
	maskDesc.Height = height;
	maskDesc.MipLevels = 1;
	maskDesc.ArraySize = 1;
	maskDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	maskDesc.SampleDesc.Count = 1;
	maskDesc.SampleDesc.Quality = 0;
	maskDesc.Usage = D3D11_USAGE_DEFAULT;
	maskDesc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;

	device->CreateTexture2D(&maskDesc, nullptr, m_pMaskTexture.GetAddressOf());
	device->CreateRenderTargetView(m_pMaskTexture.Get(), nullptr, m_pMaskRTV.GetAddressOf());
	device->CreateShaderResourceView(m_pMaskTexture.Get(), nullptr, m_pMaskSRV.GetAddressOf());

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

	// Viewport
	D3D11_VIEWPORT viewport{};
	viewport.TopLeftX = 0.0f;
	viewport.TopLeftY = 0.0f;
	viewport.Width = static_cast<float>(m_width);
	viewport.Height = static_cast<float>(m_height);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;
	context->RSSetViewports(1, &viewport);

	// 셰이더 로딩 (프로젝트 로드 이후)
	if ((!m_pPickingVS || !m_pPickingPS || !m_pMaskPS || !m_pFullScreenVS || !m_pOutlinePS) &&
		!ResourceManager::Get().GetCurrentRootPath().empty())
	{
		if (!m_pPickingVS)
			m_pPickingVS = ResourceManager::Get().Load<VShader>(L"Shader/PBR/VS/SkeletalVertexShader.hlsl");
		if (!m_pPickingPS)
			m_pPickingPS = ResourceManager::Get().Load<PShader>(L"Shader/Editor/PickingPS.hlsl");
		if (!m_pMaskPS)
			m_pMaskPS = ResourceManager::Get().Load<PShader>(L"Shader/Editor/MaskPS.hlsl");
		if (!m_pFullScreenVS)
			m_pFullScreenVS = ResourceManager::Get().Load<VShader>(L"Shader/PP/FullScreenVS.hlsl");
		if (!m_pOutlinePS)
			m_pOutlinePS = ResourceManager::Get().Load<PShader>(L"Shader/Editor/OutlinePS.hlsl");
	}

	m_pCam->UpdateProjectionBlend();
	if (m_isFocused && !m_blockCameraInput)
		m_pCam->InputUpdate((int)m_guizmoOperation);

	auto view = m_pCam->GetViewMatrix();
	auto proj = m_pCam->GetProjMatrix();

	RenderManager::Get().SetViewMatrix(view);
	RenderManager::Get().SetProjMatrix(proj);

	// ID 텍스쳐 렌더링
	if (m_pPickingVS && m_pPickingPS && m_pPickingIdBuffer)
	{
		context->OMSetRenderTargets(1, m_pIdRTV.GetAddressOf(), dsv);
		float idClear[4] = { 0, 0, 0, 0 };
		context->ClearRenderTargetView(m_pIdRTV.Get(), idClear);
		context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
		context->RSSetState(m_states->CullNone());

		RenderManager::Get().RenderPickingIds(
			m_pPickingVS->m_pVShader.Get(),
			m_pPickingPS->m_pPShader.Get(),
			m_pPickingVS->m_pInputLayout.Get(),
			m_pPickingIdBuffer.Get());
	}

	// Scene 렌더링
	context->OMSetDepthStencilState(nullptr, 0);
	context->OMSetRenderTargets(1, &rtv, dsv);
	float clearColor[4] = { 0.1f, 0.1f, 0.1f, 1.0f };
	context->ClearRenderTargetView(rtv, clearColor);
	context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

	m_pGridRenderer->Render(context, *m_pCam);

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

	// Stencil 기반 마스크 생성 (선택된 오브젝트, 깊이 무시)
	if (g_selectedGameObject.IsValid() && !g_selectedGameObject->IsDestroyed()
		&& m_pPickingVS && m_pMaskPS && m_pStencilWriteState && m_pStencilTestState)
	{
		std::vector<uint32_t> selectedIds;
		auto renderers = g_selectedGameObject->GetComponents<Renderer>();
		selectedIds.reserve(renderers.size());

		for (auto& renderer : renderers)
		{
			if (!renderer.IsValid() || renderer->IsDestroyed())
				continue;

			uint32_t idx = renderer->GetRenderIndex();
			if (idx != UINT32_MAX)
				selectedIds.push_back(idx);
		}

		// 1) 스텐실 클리어
		context->ClearDepthStencilView(dsv, D3D11_CLEAR_STENCIL, 1.0f, 0);

		// 2) 선택 오브젝트를 스텐실에 기록 (컬러 쓰기 없음)
		context->OMSetRenderTargets(1, &rtv, dsv);
		context->OMSetDepthStencilState(m_pStencilWriteState.Get(), 1);
		context->OMSetBlendState(m_pNoColorWriteBS.Get(), nullptr, 0xFFFFFFFF);
		context->RSSetState(m_states->CullNone());

		if (!selectedIds.empty())
		{
			RenderManager::Get().RenderSelectedMask(
				m_pPickingVS->m_pVShader.Get(),
				m_pMaskPS->m_pPShader.Get(),
				m_pPickingVS->m_pInputLayout.Get(),
				selectedIds.data(),
				static_cast<uint32_t>(selectedIds.size()));
		}

		// 3) 스텐실 -> 마스크 텍스쳐로 변환
		context->OMSetRenderTargets(1, m_pMaskRTV.GetAddressOf(), dsv);
		context->OMSetDepthStencilState(m_pStencilTestState.Get(), 1);
		context->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
		context->ClearRenderTargetView(m_pMaskRTV.Get(), DirectX::Colors::Black);

		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);
		context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
		context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
		context->VSSetShader(m_pFullScreenVS->m_pVShader.Get(), nullptr, 0);
		context->PSSetShader(m_pMaskPS->m_pPShader.Get(), nullptr, 0);

		context->Draw(3, 0);

		context->OMSetDepthStencilState(nullptr, 0);
	}

	// 아웃라인 렌더링 (씬 뷰 전용)
	if (g_selectedGameObject.IsValid() && !g_selectedGameObject->IsDestroyed()
		&& m_pOutlinePS && m_pFullScreenVS && m_pOutlineCBuffer && m_pMaskSRV)
	{
		if (m_width > 0 && m_height > 0)
		{
			OutlineConstants constants;
			constants.color = { 1.0f, 0.5f, 0.0f, 1.0f };
			constants.texelSize = { 1.0f / static_cast<float>(m_width), 1.0f / static_cast<float>(m_height) };
			constants.thickness = 1.0f;
			constants.threshold = 0.1f;

			context->UpdateSubresource(m_pOutlineCBuffer.Get(), 0, nullptr, &constants, 0, 0);

			context->OMSetRenderTargets(1, &rtv, dsv);
			context->OMSetBlendState(m_pOutlineBlendState.Get(), nullptr, 0xffffffff);
			context->OMSetDepthStencilState(m_states->DepthNone(), 0);
			context->RSSetState(m_states->CullNone());

			context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			context->IASetInputLayout(nullptr);
			context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
			context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
			context->VSSetShader(m_pFullScreenVS->m_pVShader.Get(), nullptr, 0);
			context->PSSetShader(m_pOutlinePS->m_pPShader.Get(), nullptr, 0);

			ID3D11ShaderResourceView* maskSrv = m_pMaskSRV.Get();
			context->PSSetShaderResources(0, 1, &maskSrv);
			context->PSSetConstantBuffers(0, 1, m_pOutlineCBuffer.GetAddressOf());

			context->Draw(3, 0);

			ID3D11ShaderResourceView* nullSRV2 = nullptr;
			context->PSSetShaderResources(0, 1, &nullSRV2);
		}
	}

	// 여기서 함수 끝나면 guard 소멸자에서 원래 RT/Viewport/Blend 등 자동 복원됨
}
