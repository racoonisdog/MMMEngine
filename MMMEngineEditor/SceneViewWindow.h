#pragma once
#include "Singleton.hpp"
#include "EditorCamera.h"
#include "EditorGridRenderer.h"
#include "ImGuizmo.h"
#include "DebugDraw.h"
#include "Effects.h"
#include "CommonStates.h"

#include <memory>

#define NOMINMAX
#include <d3d11.h>
#include <wrl/client.h>

#pragma comment (lib, "d3d11.lib")

namespace MMMEngine::Editor
{
	class SceneViewWindow : public Utility::Singleton<SceneViewWindow>
	{
	public:
		void Initialize(ID3D11Device* device, ID3D11DeviceContext* context, int initWidth, int initHeight);
		void Render();

	private:
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pSceneTexture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pSceneRTV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pSceneSRV;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_pSceneDSV;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pDepthStencilBuffer;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pIdTexture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_pIdRTV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_pIdSRV;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_pIdStagingTex;

		std::unique_ptr<EditorCamera> m_pCam;
		std::unique_ptr<EditorGridRenderer> m_pGridRenderer;

		ID3D11Device* m_cachedDevice;
		ID3D11DeviceContext* m_cachedContext;

		

		int m_width;
		int m_height;
		int m_lastWidth;
		int m_lastHeight;

		using DefaultVertex = DirectX::VertexPositionColor;

		std::unique_ptr<DirectX::CommonStates> m_states;
		std::unique_ptr<DirectX::BasicEffect> m_effect;
		std::unique_ptr<DirectX::PrimitiveBatch<DefaultVertex>> m_batch;
		ComPtr<ID3D11InputLayout> m_pDebugDrawIL;

		bool m_enableDebugDraw = true;
		bool m_enableDebugDrawZbuffer = false;

		bool m_isHovered = false;
		bool m_isFocused = false;

		ImGuizmo::OPERATION m_guizmoOperation = ImGuizmo::OPERATION::TRANSLATE;
		ImGuizmo::MODE m_guizmoMode = ImGuizmo::MODE::LOCAL;

		bool CreateRenderTargets(ID3D11Device* device, int width, int height);
		void ResizeRenderTarget(ID3D11Device* device, int width, int height);
		void RenderSceneToTexture(ID3D11DeviceContext* context);
	};
}
