#pragma once
#include "Export.h"
#include "ExportSingleton.hpp"
#include <RendererBase.h>
#include <map>
#include <vector>
#include <memory>
#include <type_traits>
#include <typeindex>

#include <dxgi1_4.h>
#include <wrl/client.h>
#include <SimpleMath.h>

#include <RenderShared.h>
#include <Object.h>
#include "json/json.hpp"

#pragma comment (lib, "d3d11.lib")
#pragma comment (lib, "dxgi.lib")

namespace MMMEngine
{
	class Transform;
	class EditorCamera;
	class Material;
	class VShader;
	class PShader;
	class MMMENGINE_API RenderManager : public Utility::ExportSingleton<RenderManager>
	{
		friend class RendererBase;
		friend class Material;
	private:
		std::map<int, std::vector<std::shared_ptr<RendererBase>>> m_Passes;
		std::queue<std::shared_ptr<RendererBase>> m_initQueue;

		void ResizeRTVs(int width, int height);

	protected:
		HWND* m_pHwnd = nullptr;	// HWND 포인터
		UINT m_rClientWidth = 0;
		UINT m_rClientHeight = 0;
		int m_rSyncInterval = 1;
		
		float m_backColor[4] = { 0.0f, 0.5f, 0.5f, 1.0f };	// 백그라운드 컬러

		// 디바이스
		Microsoft::WRL::ComPtr<ID3D11Device5> m_pDevice;

		// 기본 렌더 인터페이스
		Microsoft::WRL::ComPtr<ID3D11DeviceContext4> m_pDeviceContext;		// 디바이스 컨텍스트
		Microsoft::WRL::ComPtr<IDXGISwapChain4> m_pSwapChain;				// 스왑체인

		Microsoft::WRL::ComPtr<ID3D11RenderTargetView1> m_pRenderTargetView;	// 렌더링 타겟뷰
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_pDepthStencilView;		// 깊이값 처리를 위한 뎊스스텐실 뷰
		Microsoft::WRL::ComPtr<ID3D11Texture2D1> m_pDepthStencilTexture;		// 뎊스스텐실 텍스쳐

		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_pDafaultSamplerLinear;		// 샘플러 상태.
		Microsoft::WRL::ComPtr<ID3D11RasterizerState2> m_pDefaultRS;			// 기본 RS

		Microsoft::WRL::ComPtr<ID3D11BlendState1> m_pDefaultBS;		// 기본 블랜드 스테이트
		Microsoft::WRL::ComPtr<ID3D11RasterizerState2> m_DefaultRS;	// 기본 레스터라이저 스테이트
		D3D11_VIEWPORT m_defaultViewport;							// 기본 뷰포트

		// 씬을 처음에 그릴 중간 버퍼 (HDR 처리를 위해 보통 float 포맷 사용)
		Microsoft::WRL::ComPtr<ID3D11Texture2D1>          m_pSceneTexture;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView1>   m_pSceneRTV;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView1> m_pSceneSRV;

		// 버퍼 기본색상
		DirectX::SimpleMath::Vector4 m_ClearColor;

		// 인풋 레이아웃
		std::shared_ptr<VShader> m_pDefaultVSShader;
		std::shared_ptr<PShader> m_pDefaultPSShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_pDefaultInputLayout;

		// 카메라 관련
		ObjPtr<EditorCamera> m_pCamera;
		Render_CamBuffer m_camMat;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_pCambuffer = nullptr;		// 트랜스폼 버퍼

		// 백버퍼 텍스쳐
		Microsoft::WRL::ComPtr<ID3D11Texture2D1> m_pBackBuffer = nullptr;		// 백버퍼 텍스처

		// 텍스쳐 버퍼인덱스 주는 맵 <propertyName, index>
		std::unordered_map<std::wstring, int> m_propertyMap;
	public:
		void StartUp(HWND* _hwnd, UINT _ClientWidth, UINT _ClientHeight);
		void InitD3D();
		void ShutDown();
		void Start();
		void ResizeScreen(int width, int height);
		void SetCamera(ObjPtr<EditorCamera> _cameraComp);


		void BeginFrame();
		void Render();
		void EndFrame();

		const Microsoft::WRL::ComPtr<ID3D11Device5> GetDevice() const { return m_pDevice; }
		const Microsoft::WRL::ComPtr<ID3D11DeviceContext4> GetContext() const { return m_pDeviceContext; }
		const std::shared_ptr<VShader> GetDefaultVS() const { return m_pDefaultVSShader; }
		const std::shared_ptr<PShader> GetDefaultPS() const { return m_pDefaultPSShader; }
		const Microsoft::WRL::ComPtr<ID3D11InputLayout> GetDefaultInputLayout() const { return m_pDefaultInputLayout; }
		const int PropertyToIdx(const std::wstring& _propertyName) const;
	public:
		template <typename T, typename... Args>
		std::weak_ptr<RendererBase> AddRenderer(RenderType _passType, Args&&... args) {
			std::shared_ptr<T> temp = std::make_shared<T>(std::forward<Args>(args)...);
			m_Passes[_passType].push_back(temp);
			m_initQueue.push(temp);

			return temp;
		}

		template <typename T>
		bool RemoveRenderer(MMMEngine::RenderType _passType, std::shared_ptr<T>& _renderer) {
			if (_renderer && (m_Passes.find(_passType) != m_Passes.end())) {
				auto it = std::find(m_Passes[_passType].begin(), m_Passes[_passType].end(), _renderer);

				if (it != m_Passes[_passType].end()) {
					m_Passes[_passType].erase(it);
					return true;
				}
			}

			return false;
		}
	};
}
