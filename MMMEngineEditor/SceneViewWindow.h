#pragma once
#include "Singleton.hpp"
#include "EditorCamera.h"
#include "EditorGridRenderer.h"

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

		std::unique_ptr<EditorCamera> m_pCam;
		std::unique_ptr<EditorGridRenderer> m_pGridRenderer;

		ID3D11Device* m_cachedDevice;
		ID3D11DeviceContext* m_cachedContext;

		int m_width;
		int m_height;
		int m_lastWidth;
		int m_lastHeight;

		bool m_isHovered = false;
		bool m_isFocused = false;

		bool CreateRenderTargets(ID3D11Device* device, int width, int height);
		void ResizeRenderTarget(ID3D11Device* device, int width, int height);
		void RenderSceneToTexture(ID3D11DeviceContext* context);
	};
}