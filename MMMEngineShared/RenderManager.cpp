#include "RenderManager.h"

#include "RendererTools.h"
#include "RenderShared.h"
#include "ShaderInfo.h"
#include "ResourceManager.h"
#include "GameObject.h"
#include "Transform.h"
#include "Camera.h"
#include "Renderer.h"
#include "Material.h"
#include "Canvas.h"
#include "VShader.h"
#include "PShader.h"
#include "Texture2D.h"
#include "Font.h"
#include "Text.h"

#include "rttr/registration.h"
#include <cmath>
#include <algorithm>
#include <cwctype>
#include <exception>
#include <filesystem>

DEFINE_SINGLETON(MMMEngine::RenderManager)

using namespace Microsoft::WRL;
using namespace DirectX::SimpleMath;
using namespace DirectX;

namespace
{
	struct Render_UIBuffer
	{
		Vector4 rect;
		Vector4 uvRect;
		Vector4 color;
		Vector4 screenParams; // x=width, y=height, z=useTexture, w=unused
		Vector4 maskParams; // x=maskEnabled, y=alphaThreshold, z=unused, w=unused
		Vector4 transformParams0; // x=pivotX, y=pivotY, z=rightX, w=rightY
		Vector4 transformParams1; // x=upX, y=upY, z=unused, w=unused
		Matrix viewProj;     // reserved
	};

	std::wstring FilterTextForSpriteFont(const DirectX::SpriteFont& spriteFont, const std::wstring& text)
	{
		if (text.empty())
			return {};

		wchar_t fallback = 0;
		if (spriteFont.ContainsCharacter(L'?'))
			fallback = L'?';
		else if (spriteFont.ContainsCharacter(L' '))
			fallback = L' ';

		std::wstring filtered;
		filtered.reserve(text.size());
		for (wchar_t ch : text)
		{
			if (spriteFont.ContainsCharacter(ch))
				filtered.push_back(ch);
			else if (fallback != 0)
				filtered.push_back(fallback);
		}
		return filtered;
	}

	float MeasureTextWidth(const DirectX::SpriteFont& spriteFont, const std::wstring& text)
	{
		if (text.empty())
			return 0.0f;

		return DirectX::XMVectorGetX(spriteFont.MeasureString(text.c_str(), false));
	}

	bool IsInlineWhitespace(wchar_t ch)
	{
		return ch != L'\n' && ch != L'\r' && std::iswspace(ch) != 0;
	}

	std::vector<std::wstring> WrapLineByCharacter(const DirectX::SpriteFont& spriteFont,
		const std::wstring& line, float maxWidth)
	{
		if (line.empty())
			return { std::wstring() };

		if (maxWidth <= 0.0f)
			return { line };

		std::vector<std::wstring> wrappedLines;
		std::wstring currentLine;
		for (wchar_t ch : line)
		{
			if (ch == L'\r')
				continue;

			std::wstring candidate = currentLine;
			candidate.push_back(ch);
			if (!currentLine.empty() && MeasureTextWidth(spriteFont, candidate) > maxWidth)
			{
				wrappedLines.push_back(currentLine);
				currentLine.assign(1, ch);
			}
			else
			{
				currentLine.push_back(ch);
			}

			// Keep rendering progress for glyphs wider than the wrap width.
			if (currentLine.size() == 1 && MeasureTextWidth(spriteFont, currentLine) > maxWidth)
			{
				wrappedLines.push_back(currentLine);
				currentLine.clear();
			}
		}

		if (!currentLine.empty() || wrappedLines.empty())
			wrappedLines.push_back(currentLine);

		return wrappedLines;
	}

	std::vector<std::wstring> WrapLineByWord(const DirectX::SpriteFont& spriteFont,
		const std::wstring& line, float maxWidth)
	{
		if (line.empty())
			return { std::wstring() };

		if (maxWidth <= 0.0f)
			return { line };

		std::vector<std::wstring> wrappedLines;
		std::wstring currentLine;

		size_t i = 0;
		while (i < line.size())
		{
			if (line[i] == L'\r')
			{
				++i;
				continue;
			}

			const bool whitespaceToken = IsInlineWhitespace(line[i]);
			size_t j = i + 1;
			while (j < line.size())
			{
				if (line[j] == L'\r')
				{
					++j;
					continue;
				}

				if (IsInlineWhitespace(line[j]) != whitespaceToken)
					break;
				++j;
			}

			std::wstring token;
			token.reserve(j - i);
			for (size_t k = i; k < j; ++k)
			{
				if (line[k] != L'\r')
					token.push_back(line[k]);
			}
			i = j;

			if (token.empty())
				continue;

			if (whitespaceToken)
			{
				if (currentLine.empty())
					continue;

				std::wstring candidate = currentLine + token;
				if (MeasureTextWidth(spriteFont, candidate) <= maxWidth)
					currentLine = std::move(candidate);
				else
				{
					wrappedLines.push_back(currentLine);
					currentLine.clear();
				}
				continue;
			}

			const float tokenWidth = MeasureTextWidth(spriteFont, token);
			if (currentLine.empty())
			{
				if (tokenWidth <= maxWidth)
				{
					currentLine = token;
				}
				else
				{
					auto brokenTokenLines = WrapLineByCharacter(spriteFont, token, maxWidth);
					for (size_t lineIdx = 0; lineIdx < brokenTokenLines.size(); ++lineIdx)
					{
						if (lineIdx + 1 < brokenTokenLines.size())
							wrappedLines.push_back(brokenTokenLines[lineIdx]);
						else
							currentLine = brokenTokenLines[lineIdx];
					}
				}
				continue;
			}

			std::wstring candidate = currentLine + token;
			if (MeasureTextWidth(spriteFont, candidate) <= maxWidth)
			{
				currentLine = std::move(candidate);
			}
			else
			{
				wrappedLines.push_back(currentLine);
				currentLine.clear();

				if (tokenWidth <= maxWidth)
				{
					currentLine = token;
				}
				else
				{
					auto brokenTokenLines = WrapLineByCharacter(spriteFont, token, maxWidth);
					for (size_t lineIdx = 0; lineIdx < brokenTokenLines.size(); ++lineIdx)
					{
						if (lineIdx + 1 < brokenTokenLines.size())
							wrappedLines.push_back(brokenTokenLines[lineIdx]);
						else
							currentLine = brokenTokenLines[lineIdx];
					}
				}
			}
		}

		if (!currentLine.empty() || wrappedLines.empty())
			wrappedLines.push_back(currentLine);

		return wrappedLines;
	}

	std::wstring WrapTextToWidth(const DirectX::SpriteFont& spriteFont, const std::wstring& text,
		MMMEngine::TextWrapMode wrapMode, float maxWidth)
	{
		if (text.empty() || wrapMode == MMMEngine::TextWrapMode::NoWrap || maxWidth <= 0.0f)
			return text;

		std::wstring wrappedText;
		bool firstLine = true;
		size_t lineStart = 0;
		while (lineStart <= text.size())
		{
			const size_t lineEnd = text.find(L'\n', lineStart);
			const std::wstring rawLine = (lineEnd == std::wstring::npos)
				? text.substr(lineStart)
				: text.substr(lineStart, lineEnd - lineStart);

			std::vector<std::wstring> wrappedLines;
			if (wrapMode == MMMEngine::TextWrapMode::CharacterWrap)
				wrappedLines = WrapLineByCharacter(spriteFont, rawLine, maxWidth);
			else
				wrappedLines = WrapLineByWord(spriteFont, rawLine, maxWidth);

			for (const auto& wrappedLine : wrappedLines)
			{
				if (!firstLine)
					wrappedText.push_back(L'\n');
				wrappedText += wrappedLine;
				firstLine = false;
			}

			if (lineEnd == std::wstring::npos)
				break;

			lineStart = lineEnd + 1;
		}

		return wrappedText;
	}
}

RTTR_REGISTRATION{
	using namespace rttr;
	using namespace MMMEngine;

	rttr::registration::class_<RenderManager>("RenderManager")
		.property("maincamera", &RenderManager::GetCamera, &RenderManager::SetCamera);
}

namespace MMMEngine {

	RenderManager::RenderManager()
	{
		m_worldMatrix = Matrix::Identity;
		m_viewMatrix = Matrix::Identity;
		m_projMatrix = Matrix::Identity;
	}

	void RenderManager::ApplyMatToContext(ID3D11DeviceContext4* _context, Material* _material)
	{
		if (!_material->GetPShader())
			return;

		auto VS = _material->GetVShader();
		auto PS = _material->GetPShader();
		ShaderType type = ShaderInfo::Get().GetShaderType(PS->GetFilePath());
		_context->VSSetShader(VS->m_pVShader.Get(), nullptr, 0);
		_context->PSSetShader(PS->m_pPShader.Get(), nullptr, 0);

		// TODO::인풋레이아웃 ShaderInfo 사용해 자동등록 시키기
		_context->IASetInputLayout(_material->GetVShader()->m_pInputLayout.Get());

		// 메테리얼
		for (auto& [prop, val] : _material->GetProperties()) {
			UpdateProperty(prop, val, type);
		}
	}

	void RenderManager::ExcuteCommands()
	{
		for (auto& [type, commands] : m_renderCommands)
		{
			if (type == RenderType::R_TRANSCULANT || type == RenderType::R_PARTICLE)
			{
				// 투명 오브젝트: 카메라 거리 내림차순 정렬
				std::sort(commands.begin(), commands.end(),
					[](const RenderCommand& a, const RenderCommand& b)
					{
						return a.camDistance > b.camDistance;
					});
			}
			else if (type == RenderType::R_SKYBOX)
			{
				if (m_pSkyboxMaterial.expired()) {
					m_pSkyboxMaterial = commands[0].material;

					if (m_pSkyboxMaterial.expired())
						continue;

					// 공용 리소스 등록
					for (auto& [prop, val] : m_pSkyboxMaterial.lock()->GetProperties())
						ShaderInfo::Get().AddGlobalPropVal(S_PBR, prop, val);
				}
			}
			else
			{
				// 불투명 오브젝트: 머티리얼 기준 정렬
				std::sort(commands.begin(), commands.end(),
					[](const RenderCommand& a, const RenderCommand& b)
					{
						if (!a.material || !b.material)
							return false;
						return a.material < b.material;
					});
			}

			float blendFactor[4] = { 0,0,0,0 };
			if (type == RenderType::R_PARTICLE)
				m_pDeviceContext->OMSetBlendState(m_pUIBlendState.Get(), blendFactor, 0xffffffff);
			else
				m_pDeviceContext->OMSetBlendState(m_pDefaultBS.Get(), blendFactor, 0xffffffff);

			if (type == RenderType::R_PARTICLE)
				m_pDeviceContext->RSSetState(m_pUIRS ? m_pUIRS.Get() : m_pDefaultRS.Get());
			else
				m_pDeviceContext->RSSetState(m_pDefaultRS.Get());

			// 정렬된 커맨드 실행
			ResPtr<Material> lastMaterial;
			Mesh_BoneBuffer* lastOffset = nullptr;
			Mesh_BoneBuffer* lastAnim = nullptr;
			for (auto& cmd : commands)
			{
				if (cmd.material == nullptr)
					continue;

				if (cmd.material != lastMaterial)
				{
					ApplyMatToContext(m_pDeviceContext.Get(), cmd.material.get());
					lastMaterial = cmd.material;
				}


				UINT stride = sizeof(Mesh_Vertex); // 실제 버텍스 구조체 크기
				UINT offset = 0;
				m_pDeviceContext->IASetVertexBuffers(0, 1, &cmd.vertexBuffer, &stride, &offset);
				m_pDeviceContext->IASetIndexBuffer(cmd.indexBuffer, DXGI_FORMAT_R32_UINT, 0);


				// 스킨드 메시라면 본 인덱스를 셰이더에 전달
				if (cmd.offsetBuffer != nullptr && lastOffset != cmd.offsetBuffer)
				{

					m_pDeviceContext->UpdateSubresource1(m_pOffsetBuffer.Get(), 0, nullptr, cmd.offsetBuffer, 0, 0, D3D11_COPY_DISCARD);
					m_pDeviceContext->VSSetConstantBuffers(3, 1, m_pOffsetBuffer.GetAddressOf());
					lastOffset = cmd.offsetBuffer;
				}

				if (cmd.animBuffer != nullptr && lastAnim != cmd.animBuffer)
				{
					m_pDeviceContext->UpdateSubresource1(m_pAnimBuffer.Get(), 0, nullptr, cmd.animBuffer, 0, 0, D3D11_COPY_DISCARD);
					m_pDeviceContext->VSSetConstantBuffers(2, 1, m_pAnimBuffer.GetAddressOf());
					lastAnim = cmd.animBuffer;
				}

				// 상수버퍼 등록
				auto sType = ShaderInfo::Get().GetShaderType(lastMaterial->GetPShader()->GetFilePath());

				if (type == RenderType::R_PARTICLE && m_pParticleBuffer)
				{
					const float particleAlpha = cmd.useParticleAlpha ? cmd.particleAlpha : 1.0f;
					const Vector4 particleParams = { particleAlpha, 0.0f, 0.0f, 0.0f };
					m_pDeviceContext->UpdateSubresource1(m_pParticleBuffer.Get(), 0, nullptr, &particleParams, 0, 0, D3D11_COPY_DISCARD);
					m_pDeviceContext->PSSetConstantBuffers(10, 1, m_pParticleBuffer.GetAddressOf());
				}
				else if (type == RenderType::R_GEOMETRY && m_pParticleBuffer)
				{
					const float ditherAlpha = cmd.useDitherAlpha ? cmd.ditherAlpha : 1.0f;
					const Vector4 ditherParams = { ditherAlpha, 0.0f, 0.0f, 0.0f };
					m_pDeviceContext->UpdateSubresource1(m_pParticleBuffer.Get(), 0, nullptr, &ditherParams, 0, 0, D3D11_COPY_DISCARD);
					m_pDeviceContext->PSSetConstantBuffers(10, 1, m_pParticleBuffer.GetAddressOf());
				}

				// Per-renderer receiveShadow flag: bind/unbind shadow map SRV explicitly.
				PropertyInfo shadowPropInfo{};
				const int shadowSlot = ShaderInfo::Get().PropertyToIdx(sType, L"_shadowmap", &shadowPropInfo);
				if (shadowSlot >= 0 && shadowPropInfo.propertyType == PropertyType::Texture)
				{
					ID3D11ShaderResourceView* shadowSrv = nullptr;
					if (cmd.receiveShadow && m_pShadowSRV)
						shadowSrv = m_pShadowSRV->m_pSRV.Get();
					m_pDeviceContext->PSSetShaderResources(shadowSlot, 1, &shadowSrv);
				}

				// 상수버퍼 일렬업데이트
				ShaderInfo::Get().UpdateCBuffers(sType);

				// 디더링 알파: R_GEOMETRY일 때만 b10 덮어쓰기 (R_PARTICLE은 위에서 이미 particle alpha로 설정함)
				if (type == RenderType::R_GEOMETRY && m_pParticleBuffer)
				{
					const float ditherAlpha = cmd.useDitherAlpha ? cmd.ditherAlpha : 1.0f;
					const Vector4 ditherParams = { ditherAlpha, 0.0f, 0.0f, 0.0f };
					m_pDeviceContext->UpdateSubresource1(m_pParticleBuffer.Get(), 0, nullptr, &ditherParams, 0, 0, D3D11_COPY_DISCARD);
					m_pDeviceContext->PSSetConstantBuffers(10, 1, m_pParticleBuffer.GetAddressOf());
				}

				// 월드매트릭스 버퍼집어넣기
				Render_TransformBuffer transformBuffer;
				transformBuffer.mWorld = XMMatrixTranspose(m_objWorldMatMap[cmd.worldMatIndex]);
				transformBuffer.mNormalMatrix = XMMatrixInverse(nullptr, m_objWorldMatMap[cmd.worldMatIndex]);
				m_pDeviceContext->UpdateSubresource1(m_pTransbuffer.Get(), 0, nullptr, &transformBuffer, 0, 0, D3D11_COPY_DISCARD);
				m_pDeviceContext->VSSetConstantBuffers(1, 1, m_pTransbuffer.GetAddressOf());


				if (type == RenderType::R_SKYBOX)
					m_pDeviceContext->Draw(3, 0);
				else
					m_pDeviceContext->DrawIndexed(cmd.indiciesSize, 0, 0);
			}
		}
	}

	void RenderManager::ClearCache()
	{
		// 캐싱 컨테이너 초기화
		m_objWorldMatMap.clear();
		m_renderCommands.clear();

		m_rObjIdx = 0;
	}

	void RenderManager::UpdateRenderers()
	{
		for (auto& renderer : m_renderers) {
			if (renderer->IsActiveAndEnabled()) {
				renderer->Render();
			}
		}
	}

	void RenderManager::UpdateLights()
	{
		for (auto& light : m_lights) {
			if (light->IsActiveAndEnabled()) {
				light->Render();
			}
		}
	}

	void RenderManager::EnsureUIShaders()
	{
		if (m_pUIVShader && m_pUIPShader)
			return;

		if (ResourceManager::Get().GetCurrentRootPath().empty())
			return;

		if (!m_pUIVShader)
			m_pUIVShader = ResourceManager::Get().Load<VShader>(L"Shader/UI/UIQuadVS.hlsl");
		if (!m_pUIPShader)
			m_pUIPShader = ResourceManager::Get().Load<PShader>(L"Shader/UI/UIQuadPS.hlsl");
	}

	void RenderManager::EnsureUISpriteBatch()
	{
		if (m_uiSpriteBatch || !m_pDeviceContext)
			return;

		m_uiSpriteBatch = std::make_unique<DirectX::SpriteBatch>(m_pDeviceContext.Get());
	}

	DirectX::SpriteFont* RenderManager::GetSpriteFont(const ResPtr<Font>& font)
	{
		if (!font || !m_pDevice)
			return nullptr;

		std::filesystem::path root = ResourceManager::Get().GetCurrentRootPath();
		std::filesystem::path filePath = font->GetFilePath();
		std::filesystem::path fullPath = root.empty() ? filePath : (root / filePath);
		std::wstring key = fullPath.wstring();
		if (key.empty())
			return nullptr;

		auto it = m_uiSpriteFontCache.find(key);
		if (it != m_uiSpriteFontCache.end())
			return it->second.get();

		try
		{
			std::unique_ptr<DirectX::SpriteFont> spriteFont;
			if (font->HasSpriteFontData())
			{
				const auto& data = font->GetSpriteFontData();
				spriteFont = std::make_unique<DirectX::SpriteFont>(m_pDevice.Get(), data.data(), data.size());
			}
			else
			{
				spriteFont = std::make_unique<DirectX::SpriteFont>(m_pDevice.Get(), fullPath.c_str());
			}

			// Missing glyphs cause runtime_error unless a default character is set.
			if (spriteFont->ContainsCharacter(L'?'))
				spriteFont->SetDefaultCharacter(L'?');
			else if (spriteFont->ContainsCharacter(L' '))
				spriteFont->SetDefaultCharacter(L' ');

			auto* result = spriteFont.get();
			m_uiSpriteFontCache.emplace(key, std::move(spriteFont));
			return result;
		}
		catch (const std::exception&)
		{
			return nullptr;
		}
	}

	void RenderManager::RenderUI()
	{
		if (m_canvases.empty())
			return;

		EnsureUIShaders();
		if (!m_pUIVShader || !m_pUIPShader || !m_pUIBuffer)
			return;

		std::sort(m_canvases.begin(), m_canvases.end(), [](Canvas* a, Canvas* b) {
			return a->GetSortOrder() < b->GetSortOrder();
			});

		auto context = m_pDeviceContext.Get();

		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		context->IASetInputLayout(nullptr);
		context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
		context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);

		context->VSSetShader(m_pUIVShader->m_pVShader.Get(), nullptr, 0);
		context->PSSetShader(m_pUIPShader->m_pPShader.Get(), nullptr, 0);
		context->VSSetConstantBuffers(0, 1, m_pUIBuffer.GetAddressOf());
		context->PSSetConstantBuffers(0, 1, m_pUIBuffer.GetAddressOf());
		context->PSSetSamplers(0, 1, m_pLinearSampler.GetAddressOf());

		float blendFactor[4] = { 0,0,0,0 };
		context->OMSetBlendState(m_pUIBlendState.Get(), blendFactor, 0xffffffff);
		context->OMSetDepthStencilState(m_pUIDepthState.Get(), 0);
		context->RSSetState(m_pDefaultRS.Get());
		m_uiActiveDepthState = m_pUIDepthState.Get();
		m_uiStencilRef = 0;
		m_uiMaskEnabled = false;

		for (auto* canvas : m_canvases)
		{
			if (!canvas)
				continue;
			BeginCanvas(canvas);
			canvas->RenderUI(*this);
			EndCanvas();
		}

		ID3D11ShaderResourceView* nullSrv = nullptr;
		context->PSSetShaderResources(0, 1, &nullSrv);
		context->OMSetDepthStencilState(nullptr, 0);
		context->OMSetBlendState(m_pDefaultBS.Get(), blendFactor, 0xffffffff);
	}

	void RenderManager::StartUp(HWND _hwnd, UINT _ClientWidth, UINT _ClientHeight)
	{
		// 디바이스 생성
		ComPtr<ID3D11Device> device;
		D3D_FEATURE_LEVEL featureLevel;
		D3D11CreateDevice(nullptr, D3D_DRIVER_TYPE_HARDWARE, NULL,
			0, nullptr, 0, D3D11_SDK_VERSION,
			device.GetAddressOf(), &featureLevel, nullptr);

		HR_T(device.As(&m_pDevice), "Device::");

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
			nullptr, nullptr, swapChain.GetAddressOf()), "CreateSwapChain");
		HR_T(swapChain.As(&m_pSwapChain), "CreateSwapChain");

		// 컨텍스트 생성
		ComPtr<ID3D11DeviceContext3> context;
		m_pDevice->GetImmediateContext3(context.GetAddressOf());
		HR_T(context.As(&m_pDeviceContext), "GetImmediateContext3");

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

		// 뎊스 텍스쳐 생성
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

		// 뎊스스탠실 뷰 생성
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

		// UI 블렌드 스테이트 생성 (알파 블렌딩)
		D3D11_BLEND_DESC1 uiBlendDesc = {};
		uiBlendDesc.AlphaToCoverageEnable = FALSE;
		uiBlendDesc.IndependentBlendEnable = FALSE;
		uiBlendDesc.RenderTarget[0].BlendEnable = TRUE;
		uiBlendDesc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		uiBlendDesc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		uiBlendDesc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		uiBlendDesc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		uiBlendDesc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		uiBlendDesc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		uiBlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		HR_T(m_pDevice->CreateBlendState1(&uiBlendDesc, m_pUIBlendState.GetAddressOf()));

		// UI 마스크용 컬러 미작성 블렌드 스테이트
		D3D11_BLEND_DESC1 uiBlendNoColorDesc = uiBlendDesc;
		uiBlendNoColorDesc.RenderTarget[0].RenderTargetWriteMask = 0;
		HR_T(m_pDevice->CreateBlendState1(&uiBlendNoColorDesc, m_pUIBlendStateNoColor.GetAddressOf()));

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

		// UI 전용 RS (양면 렌더)
		D3D11_RASTERIZER_DESC2 uiRsDesc = defaultRsDesc;
		uiRsDesc.CullMode = D3D11_CULL_NONE;
		HR_T(m_pDevice->CreateRasterizerState2(&uiRsDesc, m_pUIRS.GetAddressOf()));

		// UI 깊이 스테이트 생성 (깊이 테스트/쓰기 비활성화)
		D3D11_DEPTH_STENCIL_DESC uiDepthDesc = {};
		uiDepthDesc.DepthEnable = FALSE;
		uiDepthDesc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		uiDepthDesc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		uiDepthDesc.StencilEnable = FALSE;
		HR_T(m_pDevice->CreateDepthStencilState(&uiDepthDesc, m_pUIDepthState.GetAddressOf()));

		// UI 스텐실 스테이트 생성
		D3D11_DEPTH_STENCIL_DESC uiStencilTestDesc = uiDepthDesc;
		uiStencilTestDesc.StencilEnable = TRUE;
		uiStencilTestDesc.StencilReadMask = 0xFF;
		uiStencilTestDesc.StencilWriteMask = 0x00;
		uiStencilTestDesc.FrontFace.StencilFunc = D3D11_COMPARISON_EQUAL;
		uiStencilTestDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_KEEP;
		uiStencilTestDesc.FrontFace.StencilFailOp = D3D11_STENCIL_OP_KEEP;
		uiStencilTestDesc.FrontFace.StencilDepthFailOp = D3D11_STENCIL_OP_KEEP;
		uiStencilTestDesc.BackFace = uiStencilTestDesc.FrontFace;
		HR_T(m_pDevice->CreateDepthStencilState(&uiStencilTestDesc, m_pUIStencilTestState.GetAddressOf()));

		D3D11_DEPTH_STENCIL_DESC uiStencilWriteDesc = uiStencilTestDesc;
		uiStencilWriteDesc.StencilWriteMask = 0xFF;
		uiStencilWriteDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_INCR_SAT;
		uiStencilWriteDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_INCR_SAT;
		HR_T(m_pDevice->CreateDepthStencilState(&uiStencilWriteDesc, m_pUIStencilWriteState.GetAddressOf()));

		D3D11_DEPTH_STENCIL_DESC uiStencilClearDesc = uiStencilTestDesc;
		uiStencilClearDesc.StencilWriteMask = 0xFF;
		uiStencilClearDesc.FrontFace.StencilPassOp = D3D11_STENCIL_OP_DECR_SAT;
		uiStencilClearDesc.BackFace.StencilPassOp = D3D11_STENCIL_OP_DECR_SAT;
		HR_T(m_pDevice->CreateDepthStencilState(&uiStencilClearDesc, m_pUIStencilClearState.GetAddressOf()));

		// 샘플러 만들기
		D3D11_SAMPLER_DESC sampDesc = {};
		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

		HR_T(m_pDevice->CreateSamplerState(&sampDesc, m_pLinearSampler.GetAddressOf()));

		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

		HR_T(m_pDevice->CreateSamplerState(&sampDesc, m_pLinearWarpSampler.GetAddressOf()));


		sampDesc.Filter = D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT; // 비교 필터
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_LESS_EQUAL; // 깊이 비교 함수
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

		HR_T(m_pDevice->CreateSamplerState(&sampDesc, m_pCompareSampler.GetAddressOf()));

		sampDesc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		sampDesc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		sampDesc.ComparisonFunc = D3D11_COMPARISON_NEVER;
		sampDesc.MinLOD = 0;
		sampDesc.MaxLOD = D3D11_FLOAT32_MAX;

		HR_T(m_pDevice->CreateSamplerState(&sampDesc, m_pPointSampler.GetAddressOf()));

		// === Scene 렌더타겟 초기화 ===
		D3D11_TEXTURE2D_DESC1 sceneColorDesc = {};
		sceneColorDesc.Width = m_clientWidth;
		sceneColorDesc.Height = m_clientHeight;
		sceneColorDesc.MipLevels = 1;
		sceneColorDesc.ArraySize = 1;
		sceneColorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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

		// 캠 버퍼 생성
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		bd.CPUAccessFlags = 0;

		bd.ByteWidth = sizeof(Render_CamBuffer);
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, m_pCambuffer.GetAddressOf()));
		bd.ByteWidth = sizeof(Render_TransformBuffer);
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pTransbuffer));
		bd.ByteWidth = sizeof(Render_ShadowBuffer);
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pShadowBuffer));
		bd.ByteWidth = sizeof(Mesh_BoneBuffer);
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pOffsetBuffer));
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, &m_pAnimBuffer));
		bd.ByteWidth = sizeof(Render_UIBuffer);
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, m_pUIBuffer.GetAddressOf()));
		bd.ByteWidth = sizeof(Vector4);
		HR_T(m_pDevice->CreateBuffer(&bd, nullptr, m_pParticleBuffer.GetAddressOf()));

		// 그림자 버퍼용
		D3D11_TEXTURE2D_DESC1 shadowDesc = {};
		shadowDesc.Width = m_shadowMapWidth;
		shadowDesc.Height = m_shadowMapHeight;
		shadowDesc.MipLevels = 1;
		shadowDesc.ArraySize = 1;
		shadowDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		shadowDesc.SampleDesc.Count = 1;
		shadowDesc.SampleDesc.Quality = 0;
		shadowDesc.Usage = D3D11_USAGE_DEFAULT;
		shadowDesc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
		shadowDesc.CPUAccessFlags = 0;
		shadowDesc.MiscFlags = 0;

		// 텍스처 생성
		HR_T(m_pDevice->CreateTexture2D1(&shadowDesc, nullptr, m_pShadowTexture.GetAddressOf()));

		// DepthStencilView
		D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc = {};
		dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
		dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
		dsvDesc.Texture2D.MipSlice = 0;

		HR_T(m_pDevice->CreateDepthStencilView(m_pShadowTexture.Get(), &dsvDesc, m_pShadowDSV.GetAddressOf()));

		// ShaderResourceView
		m_pShadowSRV = std::make_shared<Texture2D>();
		D3D11_SHADER_RESOURCE_VIEW_DESC1 srvDesc = {};
		srvDesc.Format = DXGI_FORMAT_R32_FLOAT;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MostDetailedMip = 0;
		srvDesc.Texture2D.MipLevels = 1;

		HR_T(m_pDevice->CreateShaderResourceView1(m_pShadowTexture.Get(), &srvDesc, m_pShadowSRV->m_pSRV.GetAddressOf()));

		// ShadwoViewport
		m_shadowVP.TopLeftX = 0.0f;
		m_shadowVP.TopLeftY = 0.0f;
		m_shadowVP.Width = (FLOAT)m_shadowMapWidth;
		m_shadowVP.Height = (FLOAT)m_shadowMapHeight;
		m_shadowVP.MinDepth = 0.0f;
		m_shadowVP.MaxDepth = 1.0f;
	}
	void RenderManager::ShutDown()
	{
		// 1) 렌더가 돌고 있을 수 있으니, 가능하면 외부에서 Render loop를 멈춘 뒤 호출하는 게 정석
	//    여기서는 내부에서 최대한 안전하게 정리.

	// 2) 컨텍스트 바인딩 해제 (가장 중요!)
		if (m_pDeviceContext)
		{
			// RenderTargets / Depth
			ID3D11RenderTargetView* nullRTV[8] = {};
			m_pDeviceContext->OMSetRenderTargets(8, nullRTV, nullptr);

			// ShaderResourceView (VS/PS/GS/HS/DS/CS)
			ID3D11ShaderResourceView* nullSRV[16] = {};
			m_pDeviceContext->VSSetShaderResources(0, 16, nullSRV);
			m_pDeviceContext->PSSetShaderResources(0, 16, nullSRV);
			m_pDeviceContext->GSSetShaderResources(0, 16, nullSRV);
			m_pDeviceContext->HSSetShaderResources(0, 16, nullSRV);
			m_pDeviceContext->DSSetShaderResources(0, 16, nullSRV);
			m_pDeviceContext->CSSetShaderResources(0, 16, nullSRV);

			// Sampler
			ID3D11SamplerState* nullSamp[16] = {};
			m_pDeviceContext->VSSetSamplers(0, 16, nullSamp);
			m_pDeviceContext->PSSetSamplers(0, 16, nullSamp);
			m_pDeviceContext->CSSetSamplers(0, 16, nullSamp);

			// ConstantBuffer
			ID3D11Buffer* nullCB[16] = {};
			m_pDeviceContext->VSSetConstantBuffers(0, 16, nullCB);
			m_pDeviceContext->PSSetConstantBuffers(0, 16, nullCB);
			m_pDeviceContext->GSSetConstantBuffers(0, 16, nullCB);
			m_pDeviceContext->HSSetConstantBuffers(0, 16, nullCB);
			m_pDeviceContext->DSSetConstantBuffers(0, 16, nullCB);
			m_pDeviceContext->CSSetConstantBuffers(0, 16, nullCB);

			// Shaders
			m_pDeviceContext->VSSetShader(nullptr, nullptr, 0);
			m_pDeviceContext->PSSetShader(nullptr, nullptr, 0);
			m_pDeviceContext->GSSetShader(nullptr, nullptr, 0);
			m_pDeviceContext->HSSetShader(nullptr, nullptr, 0);
			m_pDeviceContext->DSSetShader(nullptr, nullptr, 0);
			m_pDeviceContext->CSSetShader(nullptr, nullptr, 0);

			// Input Assembler
			m_pDeviceContext->IASetInputLayout(nullptr);
			m_pDeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
			m_pDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_UNKNOWN, 0);
			m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED);

			// States
			m_pDeviceContext->RSSetState(nullptr);
			m_pDeviceContext->OMSetBlendState(nullptr, nullptr, 0xFFFFFFFF);
			m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);

			// 뭔가 남아있으면 정리
			//m_pDeviceContext->ClearState();
			//m_pDeviceContext->Flush();
		}

		// 3) 스왑체인(전체화면 가능성 대비)
		if (m_pSwapChain)
		{
			// fullscreen 상태면 창모드로 돌려놓는 게 안전
			BOOL fs = FALSE;
			Microsoft::WRL::ComPtr<IDXGIOutput> out;
			if (SUCCEEDED(m_pSwapChain->GetFullscreenState(&fs, out.GetAddressOf())) && fs)
			{
				m_pSwapChain->SetFullscreenState(FALSE, nullptr);
			}
		}

		// 4) CPU측 캐시/참조 정리 (소유 X -> clear만)
		m_renderCommands.clear();
		m_objWorldMatMap.clear();
		m_renderers.clear();
		m_rendererIdMap.clear();
		m_rObjIdx = 0;
		m_nextRendererId = 1;

		m_canvases.clear();
		m_lights.clear();
		m_pSkyboxMaterial.reset();

		// 5) UI 리소스
		m_uiSpriteFontCache.clear();
		m_uiSpriteBatch.reset();

		m_pUIBlendState.Reset();
		m_pUIBlendStateNoColor.Reset();
		m_pUIDepthState.Reset();
		m_pUIStencilTestState.Reset();
		m_pUIStencilWriteState.Reset();
		m_pUIStencilClearState.Reset();
		m_pUIBuffer.Reset();
		m_pUIVShader.reset();
		m_pUIPShader.reset();

		// 6) 트랜스폼/카메라/스킨/쉐도우 버퍼
		m_pTransbuffer.Reset();
		m_pCambuffer.Reset();
		m_pParticleBuffer.Reset();

		m_pOffsetBuffer.Reset();
		m_pAnimBuffer.Reset();

		m_pShadowBuffer.Reset();
		m_pShadowDSV.Reset();
		m_pShadowTexture.Reset();
		m_pShadowSRV.reset();   // ResPtr<Texture2D>
		// (쉐도우 viewport는 값형이니 굳이 안 해도 되지만)
		ZeroMemory(&m_shadowVP, sizeof(m_shadowVP));

		// 7) 씬 렌더 타겟들
		m_pSceneRTV.Reset();
		m_pSceneSRV.Reset();
		m_pSceneDSV.Reset();
		m_pSceneDSB.Reset();
		m_pSceneTexture.Reset();
		ZeroMemory(&m_sceneViewport, sizeof(m_sceneViewport));

		// 8) 백버퍼 타겟/DS
		m_pRenderTargetView.Reset();
		m_pDepthStencilView.Reset();
		m_pDepthStencilBuffer.Reset();
		ZeroMemory(&m_swapViewport, sizeof(m_swapViewport));

		// 9) 공용 상태들
		m_pLinearSampler.Reset();
		m_pCompareSampler.Reset();
		m_pPointSampler.Reset();
		m_pDefaultRS.Reset();
		m_pUIRS.Reset();
		m_pDefaultBS.Reset();
		m_DefaultRS.Reset();

		// 10) 카메라/행렬/윈도우/사이즈 값 초기화
		m_pMainCamera.Reset();
		m_worldMatrix = DirectX::SimpleMath::Matrix::Identity;
		m_viewMatrix = DirectX::SimpleMath::Matrix::Identity;
		m_projMatrix = DirectX::SimpleMath::Matrix::Identity;

		m_hWnd = nullptr;
		m_clientWidth = m_clientHeight = 0;
		m_sceneWidth = m_sceneHeight = 0;
		useBackBuffer = false;
		isOrtho = false;

		// 11) 디바이스/컨텍스트/스왑체인 해제는 맨 마지막
		m_pSwapChain.Reset();
		m_pDeviceContext.Reset();
		m_pDevice.Reset();
	}
	void RenderManager::Start()
	{
		// 버퍼 기본색상
		m_ClearColor = DirectX::SimpleMath::Vector4(0.45f, 0.55f, 0.60f, 1.00f);
	}

	void RenderManager::UpdateProperty(const std::wstring& _propName, const PropertyValue& _value, ShaderType _type)
	{
		std::visit([&](auto&& arg)
			{
				using T = std::decay_t<decltype(arg)>;

				if constexpr (std::is_same_v<T, int> ||
					std::is_same_v<T, float> ||
					std::is_same_v<T, DirectX::SimpleMath::Vector3> ||
					std::is_same_v<T, DirectX::SimpleMath::Vector4> ||
					std::is_same_v<T, DirectX::SimpleMath::Matrix>)
				{
					ShaderInfo::Get().UpdateProperty(m_pDeviceContext.Get(), _type, _propName, &arg);
				}
				else if constexpr (std::is_same_v<T, ResPtr<MMMEngine::Texture2D>>)
				{
					if (arg == nullptr)
						return;

					ID3D11ShaderResourceView* srv = arg->m_pSRV.Get();
					ShaderInfo::Get().UpdateProperty(m_pDeviceContext.Get(), _type, _propName, srv);
				}
			}, _value);
	}

	void RenderManager::SetWorldMatrix(const DirectX::SimpleMath::Matrix& _world)
	{
		m_worldMatrix = _world;
	}

	void RenderManager::SetViewMatrix(const DirectX::SimpleMath::Matrix& _view)
	{
		m_viewMatrix = _view;
	}

	void RenderManager::SetProjMatrix(const DirectX::SimpleMath::Matrix& _proj)
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

	void RenderManager::ResizeSceneSize(int _sceneWidth, int _sceneHeight)
	{
		m_sceneWidth = _sceneWidth;
		m_sceneHeight = _sceneHeight;

		// 기존 리소스 해제
		if (m_pSceneRTV) { m_pSceneRTV->Release(); }
		if (m_pSceneTexture) { m_pSceneTexture->Release(); }
		if (m_pSceneSRV) { m_pSceneSRV->Release(); }
		if (m_pSceneDSV) { m_pSceneDSV->Release(); }
		if (m_pSceneDSB) { m_pSceneDSB->Release(); }

		// 컬러 텍스처 설명
		D3D11_TEXTURE2D_DESC1 colorDesc = {};
		colorDesc.Width = _sceneWidth;
		colorDesc.Height = _sceneHeight;
		colorDesc.MipLevels = 1;
		colorDesc.ArraySize = 1;
		colorDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // HDR 지원 포맷X
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
		depthDesc.Width = _sceneWidth;
		depthDesc.Height = _sceneHeight;
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


		// 뷰포트 갱신
		m_sceneViewport.Width = static_cast<float>(_sceneWidth);
		m_sceneViewport.Height = static_cast<float>(_sceneHeight);
		m_sceneViewport.MinDepth = 0.0f;
		m_sceneViewport.MaxDepth = 1.0f;
		m_sceneViewport.TopLeftX = 0.0f;
		m_sceneViewport.TopLeftY = 0.0f;

		// todo : 렌더러 작업자에게 꼭 고지하기
		// 카메라 Aspect Ratio 변경
		if (m_pMainCamera.IsValid())
		{
			m_pMainCamera->SetAspect(static_cast<float>(_sceneWidth) / static_cast<float>(_sceneHeight));
		}
	}

	bool RenderManager::GetSceneDisplayRect(SceneViewportRect& outRect) const
	{
		if (m_clientWidth == 0 || m_clientHeight == 0 || m_sceneWidth == 0 || m_sceneHeight == 0)
			return false;

		if (!useBackBuffer)
		{
			outRect.x = 0.0f;
			outRect.y = 0.0f;
			outRect.width = static_cast<float>(m_sceneWidth);
			outRect.height = static_cast<float>(m_sceneHeight);
			return true;
		}

		const float sceneAspect = static_cast<float>(m_sceneWidth) / static_cast<float>(m_sceneHeight);
		const float swapchainAspect = static_cast<float>(m_clientWidth) / static_cast<float>(m_clientHeight);

		float drawWf = 0.0f;
		float drawHf = 0.0f;

		if (swapchainAspect > sceneAspect)
		{
			drawHf = static_cast<float>(m_clientHeight);
			drawWf = drawHf * sceneAspect;
		}
		else
		{
			drawWf = static_cast<float>(m_clientWidth);
			drawHf = drawWf / sceneAspect;
		}

		const int drawW = static_cast<int>(std::round(drawWf));
		const int drawH = static_cast<int>(std::round(drawHf));
		const int offsetX = (static_cast<int>(m_clientWidth) - drawW) / 2;
		const int offsetY = (static_cast<int>(m_clientHeight) - drawH) / 2;

		outRect.x = static_cast<float>(offsetX);
		outRect.y = static_cast<float>(offsetY);
		outRect.width = static_cast<float>(drawW);
		outRect.height = static_cast<float>(drawH);
		return true;
	}

	void RenderManager::AddCommand(RenderType _type, RenderCommand&& _command)
	{
		m_renderCommands[_type].push_back(std::move(_command));
	}

	int RenderManager::AddMatrix(const DirectX::SimpleMath::Matrix& _worldMatrix)
	{
		int index = m_rObjIdx++;
		m_objWorldMatMap[index] = _worldMatrix;

		return index;
	}

	void RenderManager::BeginFrame()
	{
		// Clear
		m_pDeviceContext->ClearRenderTargetView(m_pRenderTargetView.Get(), m_backColor);
		m_pDeviceContext->ClearDepthStencilView(m_pDepthStencilView.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// TODO :: 글로벌 쉐이더인포 삭제하기 (라이트는 관리했는데 스카이박스 데이터는 관리안함 바꾸셈)
		ShaderInfo::Get().ClearWorldPropertyDatas();

		if (m_pMainCamera.IsValid())
		{
			m_viewMatrix = m_pMainCamera->GetViewMatrix();
			m_projMatrix = m_pMainCamera->GetProjMatrix();
		}

		// 렌더러 컨트롤
		UpdateRenderers();
		UpdateLights();

		// 카메라 유효성 확인
		//if(!m_pMainCamera.IsValid())
		//	m_pMainCamera = Camera::GetMainCamera();
		//if (!m_pMainCamera.IsValid()) {
		//	m_pMainCamera = Camera::CreateMainCamera()->GetComponent<Camera>();
		//}
	}

	void RenderManager::ShadowRender(const DirectX::SimpleMath::Matrix& _camView)
	{
		bool flag = false;
		for (auto& light : m_lights) {
			if (light->IsActiveAndEnabled()) {
				flag = true;
				break;
			}
		}

		if (!flag)
			return;

		// 버퍼데이터 생성
		Render_ShadowBuffer shadowBuffer;

		// 라이트 방향 (정규화)
		DirectX::SimpleMath::Vector3 lightDir = DirectX::SimpleMath::Vector3::Zero;

		// 글로벌 프로퍼티 찾기
		for (int i = 0; i < static_cast<int>(ShaderType::S_END); ++i) {
			ShaderType type = static_cast<ShaderType>(i);
			auto& propval = ShaderInfo::Get().GetGlobalPropVal(type, L"mLightDir");

			if (auto p = std::get_if<DirectX::SimpleMath::Vector3>(&propval)) {
				lightDir = *p;
				break;
			}
		}
		lightDir.Normalize();

		if (lightDir == Vector3::Zero)
			return;

		// 라이트 정보
		DirectX::XMMATRIX invView = XMMatrixInverse(nullptr, _camView);
		DirectX::XMVECTOR camPos = invView.r[3];
		DirectX::SimpleMath::Vector3 target = camPos;

		DirectX::SimpleMath::Vector3 lightPos = camPos;
		auto offset = (-lightDir * 500.0f);
		lightPos += offset;

		// 그림자 프로퍼티 전달
		ShaderInfo::Get().AddAllGlobalPropVal(L"mLightPos", lightPos);

		DirectX::SimpleMath::Vector3 up{ 0.0f, 1.0f, 0.0f };

		shadowBuffer.ShadowView = XMMatrixTranspose(DirectX::XMMatrixLookAtLH(lightPos, target, up));

		// 직교 투영 행렬 (쉐도우맵 범위 설정)
		float orthoWidth = 64.0f;   // 그림자 범위 (씬 크기에 맞게 조정)
		float orthoHeight = 64.0f;
		float nearZ = 100.0f;
		float farZ = 1000.0f;

		shadowBuffer.ShadowProjection =
			XMMatrixTranspose(
				XMMatrixOrthographicLH(
					orthoWidth,
					orthoHeight,
					nearZ,
					farZ
				)
			);

		// -- 렌더 설정 --
		// 캠버퍼 업데이트
		Render_CamBuffer m_camMat;
		m_camMat.mView = shadowBuffer.ShadowView;
		m_camMat.mProjection = shadowBuffer.ShadowProjection;
		m_camMat.camPos = { lightPos.x, lightPos.y, lightPos.z , 1.0f };
		m_camMat.mInvProjection = shadowBuffer.ShadowProjection.Invert();

		// RTV는 nullptr, DSV만 설정
		m_pDeviceContext->OMSetRenderTargets(0, nullptr, m_pShadowDSV.Get());

		// 깊이 버퍼 클리어
		m_pDeviceContext->ClearDepthStencilView(m_pShadowDSV.Get(), D3D11_CLEAR_DEPTH, 1.0f, 0);

		// 기본 렌더셋팅
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		float blendFactor[4] = { 0,0,0,0 };
		UINT sampleMask = 0xffffffff;
		m_pDeviceContext->OMSetBlendState(m_pDefaultBS.Get(), blendFactor, sampleMask);

		m_pDeviceContext->RSSetViewports(1, &m_shadowVP);
		ID3D11SamplerState* samplers[] = { m_pLinearSampler.Get(), m_pCompareSampler.Get(), m_pPointSampler.Get() };
		m_pDeviceContext->PSSetSamplers(0, 3, samplers);
		m_pDeviceContext->RSSetState(m_pDefaultRS.Get());

		// 리소스 업데이트
		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);
		m_pDeviceContext->UpdateSubresource1(m_pShadowBuffer.Get(), 0, nullptr, &shadowBuffer, 0, 0, D3D11_COPY_DISCARD);

		// 셰이더에 바인딩
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->VSSetConstantBuffers(4, 1, m_pShadowBuffer.GetAddressOf());
		m_pDeviceContext->PSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());

		for (auto& [type, commands] : m_renderCommands)
		{
			if (type == RenderType::R_SHADOWMAP ||
				type == RenderType::R_PREDEPTH ||
				type == RenderType::R_SKYBOX ||
				type == RenderType::R_PARTICLE ||
				type == RenderType::R_POSTPROCESS ||
				type == RenderType::R_UI ||
				type == RenderType::R_NONE ||
				type == RenderType::R_END)
			{
				// 쉐도우 안그리는거는 스킵
				continue;
			}

			if (type == RenderType::R_TRANSCULANT)
			{
				// 투명 오브젝트: 카메라 거리 내림차순 정렬
				std::sort(commands.begin(), commands.end(),
					[](const RenderCommand& a, const RenderCommand& b)
					{
						return a.camDistance > b.camDistance;
					});
			}
			else
			{
				// 불투명 오브젝트: 머티리얼 기준 정렬
				std::sort(commands.begin(), commands.end(),
					[](const RenderCommand& a, const RenderCommand& b)
					{
						if (!a.material || !b.material)
							return false;
						return a.material < b.material;
					});
			}

			// 정렬된 커맨드 실행
			ResPtr<Material> lastMaterial;
			Mesh_BoneBuffer* lastOffset = nullptr;
			Mesh_BoneBuffer* lastAnim = nullptr;
			for (auto& cmd : commands)
			{
				if (!cmd.material)
					continue;

				// CastShadow False Skip
				if (!cmd.castShadow)
					continue;

				if (cmd.material != lastMaterial)
				{
					auto VS = cmd.material->m_pVShader;
					auto PS = ShaderInfo::Get().GetShadowPShader();

					m_pDeviceContext->VSSetShader(VS->m_pVShader.Get(), nullptr, 0);
					m_pDeviceContext->PSSetShader(PS->m_pPShader.Get(), nullptr, 0);

					// 자동등록 시키기
					m_pDeviceContext->IASetInputLayout(VS->m_pInputLayout.Get());

					// Albedo 등록
					auto tex2D = std::get_if<ResPtr<Texture2D>>(&cmd.material->GetProperty(L"_albedo"));
					if ((*tex2D)) {
						ID3D11ShaderResourceView* albedo = (*tex2D)->m_pSRV.Get();
						m_pDeviceContext->PSSetShaderResources(0, 1, &albedo);
					}

					lastMaterial = cmd.material;
				}

				UINT stride = sizeof(Mesh_Vertex); // 실제 버텍스 구조체 크기
				UINT offset = 0;
				m_pDeviceContext->IASetVertexBuffers(0, 1, &cmd.vertexBuffer, &stride, &offset);
				m_pDeviceContext->IASetIndexBuffer(cmd.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

				// 스킨드 메시라면 본 인덱스를 셰이더에 전달
				if (cmd.offsetBuffer != nullptr && lastOffset != cmd.offsetBuffer)
				{

					m_pDeviceContext->UpdateSubresource1(m_pOffsetBuffer.Get(), 0, nullptr, cmd.offsetBuffer, 0, 0, D3D11_COPY_DISCARD);
					m_pDeviceContext->VSSetConstantBuffers(3, 1, m_pOffsetBuffer.GetAddressOf());
					lastOffset = cmd.offsetBuffer;
				}

				if (cmd.animBuffer != nullptr && lastAnim != cmd.animBuffer)
				{
					m_pDeviceContext->UpdateSubresource1(m_pAnimBuffer.Get(), 0, nullptr, cmd.animBuffer, 0, 0, D3D11_COPY_DISCARD);
					m_pDeviceContext->VSSetConstantBuffers(2, 1, m_pAnimBuffer.GetAddressOf());
					lastAnim = cmd.animBuffer;
				}

				// 디더 알파: 그림자 맵에도 적용 — 디더링된 메쉬가 드리우는 그림자도 디더링
				if (m_pParticleBuffer)
				{
					const float ditherAlpha = cmd.useDitherAlpha ? cmd.ditherAlpha : 1.0f;
					const Vector4 ditherParams = { ditherAlpha, 0.0f, 0.0f, 0.0f };
					m_pDeviceContext->UpdateSubresource1(m_pParticleBuffer.Get(), 0, nullptr, &ditherParams, 0, 0, D3D11_COPY_DISCARD);
					m_pDeviceContext->PSSetConstantBuffers(10, 1, m_pParticleBuffer.GetAddressOf());
				}

				// 월드매트릭스 버퍼집어넣기
				Render_TransformBuffer transformBuffer;
				transformBuffer.mWorld = XMMatrixTranspose(m_objWorldMatMap[cmd.worldMatIndex]);
				transformBuffer.mNormalMatrix = XMMatrixInverse(nullptr, m_objWorldMatMap[cmd.worldMatIndex]);
				m_pDeviceContext->UpdateSubresource1(m_pTransbuffer.Get(), 0, nullptr, &transformBuffer, 0, 0, D3D11_COPY_DISCARD);
				m_pDeviceContext->VSSetConstantBuffers(1, 1, m_pTransbuffer.GetAddressOf());

				m_pDeviceContext->DrawIndexed(cmd.indiciesSize, 0, 0);
			}
		}

		// 글로벌 쉐도우맵 추가
		ShaderInfo::Get().AddAllGlobalPropVal(L"_shadowmap", m_pShadowSRV);
	}

	void RenderManager::Render()
	{
		if (!m_pMainCamera.IsValid())
		{
			// Clear
			m_pDeviceContext->ClearRenderTargetView(m_pSceneRTV.Get(), m_backColor);
			m_pDeviceContext->ClearDepthStencilView(m_pSceneDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

			m_pDeviceContext->RSSetViewports(1, &m_sceneViewport);
			m_pDeviceContext->OMSetRenderTargets(1, reinterpret_cast<ID3D11RenderTargetView* const*>(m_pSceneRTV.GetAddressOf()), m_pSceneDSV.Get());
			RenderUI();

			m_pDeviceContext->OMSetRenderTargets(1, reinterpret_cast<ID3D11RenderTargetView* const*>(m_pRenderTargetView.GetAddressOf()), nullptr);
			return;
		}

		// Clear
		m_pDeviceContext->ClearRenderTargetView(m_pSceneRTV.Get(), m_backColor);
		m_pDeviceContext->ClearDepthStencilView(m_pSceneDSV.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);

		// 뷰 매트릭스 생성
		auto view = XMMatrixTranspose(m_pMainCamera->GetViewMatrix());

		// 그림자 렌더링
		ShadowRender(view);

		// 캠 버퍼 업데이트
		Render_CamBuffer m_camMat = {};
		m_camMat.mView = view;
		m_camMat.mProjection = XMMatrixTranspose(m_pMainCamera->GetProjMatrix());
		m_camMat.camPos = XMMatrixInverse(nullptr, m_pMainCamera->GetViewMatrix()).r[3];
		m_camMat.mInvProjection = XMMatrixTranspose(m_pMainCamera->GetProjMatrix().Invert());

		// 리소스 업데이트
		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);

		// 기본 렌더셋팅
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->PSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		float blendFactor[4] = { 0,0,0,0 };
		UINT sampleMask = 0xffffffff;
		m_pDeviceContext->OMSetBlendState(m_pDefaultBS.Get(), blendFactor, sampleMask);

		m_pDeviceContext->RSSetViewports(1, &m_sceneViewport);
		m_pDeviceContext->RSSetState(m_pDefaultRS.Get());
		m_pDeviceContext->OMSetRenderTargets(1, reinterpret_cast<ID3D11RenderTargetView* const*>(m_pSceneRTV.GetAddressOf()), m_pSceneDSV.Get());

		// 렌더커맨드 소팅, 실행
		ExcuteCommands();

		// UI 렌더링
		RenderUI();

		// 씬렌더 해제
		m_pDeviceContext->RSSetViewports(1, &m_swapViewport);
		m_pDeviceContext->OMSetRenderTargets(1, reinterpret_cast<ID3D11RenderTargetView* const*>(m_pRenderTargetView.GetAddressOf()), nullptr);

		// (풀스크린 트라이앵글)
		if (useBackBuffer) {
			auto& vs = ShaderInfo::Get().GetFullScreenVShader();
			auto& ps = ShaderInfo::Get().GetFullScreenPShader();
			m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			m_pDeviceContext->IASetInputLayout(nullptr);
			m_pDeviceContext->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
			m_pDeviceContext->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
			m_pDeviceContext->VSSetShader(vs->m_pVShader.Get(), nullptr, 0);
			m_pDeviceContext->PSSetShader(ps->m_pPShader.Get(), nullptr, 0);

			ID3D11ShaderResourceView* sceneSRV = m_pSceneSRV.Get();
			m_pDeviceContext->PSSetShaderResources(0, 1, &sceneSRV);

			ID3D11SamplerState* samplers[] = { m_pLinearWarpSampler.Get(), m_pCompareSampler.Get(), m_pPointSampler.Get() };
			m_pDeviceContext->PSSetSamplers(0, 3, samplers);

			// 씬 뷰포트 설정
			float sceneAspect = static_cast<float>(m_sceneWidth) / static_cast<float>(m_sceneHeight);
			float swapchainAspect = static_cast<float>(m_clientWidth) / static_cast<float>(m_clientHeight);

			float drawWf, drawHf;

			if (swapchainAspect > sceneAspect) {
				drawHf = static_cast<float>(m_clientHeight);
				drawWf = static_cast<float>(m_clientHeight) * sceneAspect;
			}
			else {
				drawWf = static_cast<float>(m_clientWidth);
				drawHf = static_cast<float>(m_clientWidth) / sceneAspect;
			}

			// 정수 픽셀 기준으로 스냅
			int drawW = static_cast<int>(std::round(drawWf));
			int drawH = static_cast<int>(std::round(drawHf));
			int offsetX = (static_cast<int>(m_clientWidth) - drawW) / 2;
			int offsetY = (static_cast<int>(m_clientHeight) - drawH) / 2;

			m_swapViewport.TopLeftX = static_cast<float>(offsetX);
			m_swapViewport.TopLeftY = static_cast<float>(offsetY);
			m_swapViewport.Width = static_cast<float>(drawW);
			m_swapViewport.Height = static_cast<float>(drawH);
			m_swapViewport.MinDepth = 0.0f;
			m_swapViewport.MaxDepth = 1.0f;

			// 변경된 뷰포트를 실제 파이프라인에 반영
			m_pDeviceContext->RSSetViewports(1, &m_swapViewport);

			m_pDeviceContext->Draw(3, 0);
		}
	}

	void RenderManager::RenderOnlyRenderer()
	{
		// 뷰 매트릭스 생성
		auto view = XMMatrixTranspose(m_viewMatrix);

		// 그림자 렌더링 ??왜 안댐
		//ShadowRender(view);

		// 캠 버퍼 업데이트
		Render_CamBuffer m_camMat = {};
		m_camMat.camPos = XMMatrixInverse(nullptr, m_viewMatrix).r[3];
		m_camMat.mView = view;
		m_camMat.mProjection = XMMatrixTranspose(m_projMatrix);
		m_camMat.mInvProjection = XMMatrixTranspose(m_projMatrix.Invert());

		// 리소스 업데이트
		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);

		// ID 만드는 

		// 기본 렌더셋팅
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->PSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		float blendFactor[4] = { 0,0,0,0 };
		UINT sampleMask = 0xffffffff;
		m_pDeviceContext->OMSetBlendState(m_pDefaultBS.Get(), blendFactor, sampleMask);

		m_pDeviceContext->RSSetState(m_pDefaultRS.Get());

		// RenderPass
		ExcuteCommands();
	}

	void RenderManager::RefreshRenderCommands()
	{
		ClearCache();
		UpdateRenderers();
		UpdateLights();
	}

	void RenderManager::RenderUIWithSize(UINT width, UINT height)
	{
		if (width == 0 || height == 0)
			return;

		const UINT prevWidth = m_sceneWidth;
		const UINT prevHeight = m_sceneHeight;
		m_sceneWidth = width;
		m_sceneHeight = height;

		RenderUI();

		m_sceneWidth = prevWidth;
		m_sceneHeight = prevHeight;
	}

	void RenderManager::RenderPickingIds(ID3D11PixelShader* ps, ID3D11Buffer* idBuffer)
	{
		if (!ps || !idBuffer)
			return;

		// 캠 버퍼 업데이트
		Render_CamBuffer m_camMat = {};
		m_camMat.camPos = XMMatrixInverse(nullptr, m_viewMatrix).r[3];
		m_camMat.mView = XMMatrixTranspose(m_viewMatrix);
		m_camMat.mProjection = XMMatrixTranspose(m_projMatrix);

		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);

		// 기본 렌더셋팅
		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		//m_pDeviceContext->IASetInputLayout(layout);
		//m_pDeviceContext->VSSetShader(vs, nullptr, 0);
		m_pDeviceContext->PSSetShader(ps, nullptr, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->VSSetConstantBuffers(1, 1, m_pTransbuffer.GetAddressOf());
		m_pDeviceContext->PSSetConstantBuffers(5, 1, &idBuffer);
		float blendFactor[4] = { 0,0,0,0 };
		UINT sampleMask = 0xffffffff;
		m_pDeviceContext->OMSetBlendState(m_pDefaultBS.Get(), blendFactor, sampleMask);
		m_pDeviceContext->RSSetState(m_pDefaultRS.Get());
		m_pDeviceContext->OMSetDepthStencilState(nullptr, 0);

		struct PickingIdBuffer
		{
			uint32_t objectId = 0;
			uint32_t padding[3] = { 0, 0, 0 };
		} pickData;

		for (auto& [type, commands] : m_renderCommands)
		{
			if (type == RenderType::R_SKYBOX)
				continue;

			for (auto& cmd : commands)
			{
				if (cmd.rendererID == UINT32_MAX)
					continue;

				UINT stride = sizeof(Mesh_Vertex);
				UINT offset = 0;
				m_pDeviceContext->IASetVertexBuffers(0, 1, &cmd.vertexBuffer, &stride, &offset);
				m_pDeviceContext->IASetIndexBuffer(cmd.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

				auto& matVs = cmd.material->GetVShader();
				m_pDeviceContext->VSSetShader(matVs->m_pVShader.Get(), nullptr, 0);
				m_pDeviceContext->IASetInputLayout(matVs->m_pInputLayout.Get());

				Render_TransformBuffer transformBuffer;
				transformBuffer.mWorld = XMMatrixTranspose(m_objWorldMatMap[cmd.worldMatIndex]);
				transformBuffer.mNormalMatrix = XMMatrixInverse(nullptr, m_objWorldMatMap[cmd.worldMatIndex]);
				m_pDeviceContext->UpdateSubresource1(m_pTransbuffer.Get(), 0, nullptr, &transformBuffer, 0, 0, D3D11_COPY_DISCARD);

				pickData.objectId = cmd.rendererID + 1;
				m_pDeviceContext->UpdateSubresource1(idBuffer, 0, nullptr, &pickData, 0, 0, D3D11_COPY_DISCARD);

				m_pDeviceContext->DrawIndexed(cmd.indiciesSize, 0, 0);
			}
		}
	}

	void RenderManager::RenderSelectedMask(ID3D11PixelShader* ps, const uint32_t* ids, uint32_t count)
	{
		if (!ps || !ids || count == 0)
			return;

		Render_CamBuffer m_camMat = {};
		m_camMat.camPos = XMMatrixInverse(nullptr, m_viewMatrix).r[3];
		m_camMat.mView = XMMatrixTranspose(m_viewMatrix);
		m_camMat.mProjection = XMMatrixTranspose(m_projMatrix);

		m_pDeviceContext->UpdateSubresource1(m_pCambuffer.Get(), 0, nullptr, &m_camMat, 0, 0, D3D11_COPY_DISCARD);

		m_pDeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_pDeviceContext->PSSetShader(ps, nullptr, 0);
		m_pDeviceContext->VSSetConstantBuffers(0, 1, m_pCambuffer.GetAddressOf());
		m_pDeviceContext->VSSetConstantBuffers(1, 1, m_pTransbuffer.GetAddressOf());
		// State (blend/depth/raster) is expected to be set by caller.

		auto isSelected = [ids, count](uint32_t id) -> bool
			{
				for (uint32_t i = 0; i < count; ++i)
				{
					if (ids[i] == id)
						return true;
				}
				return false;
			};

		for (auto& [type, commands] : m_renderCommands)
		{
			if (type == RenderType::R_SKYBOX)
				continue;

			for (auto& cmd : commands)
			{
				if (cmd.rendererID == UINT32_MAX || !isSelected(cmd.rendererID))
					continue;

				UINT stride = sizeof(Mesh_Vertex);
				UINT offset = 0;
				m_pDeviceContext->IASetVertexBuffers(0, 1, &cmd.vertexBuffer, &stride, &offset);
				m_pDeviceContext->IASetIndexBuffer(cmd.indexBuffer, DXGI_FORMAT_R32_UINT, 0);

				auto& matVs = cmd.material->GetVShader();
				m_pDeviceContext->VSSetShader(matVs->m_pVShader.Get(), nullptr, 0);
				m_pDeviceContext->IASetInputLayout(matVs->m_pInputLayout.Get());

				Render_TransformBuffer transformBuffer;
				transformBuffer.mWorld = XMMatrixTranspose(m_objWorldMatMap[cmd.worldMatIndex]);
				transformBuffer.mNormalMatrix = XMMatrixInverse(nullptr, m_objWorldMatMap[cmd.worldMatIndex]);
				m_pDeviceContext->UpdateSubresource1(m_pTransbuffer.Get(), 0, nullptr, &transformBuffer, 0, 0, D3D11_COPY_DISCARD);

				m_pDeviceContext->DrawIndexed(cmd.indiciesSize, 0, 0);
			}
		}
	}

	void RenderManager::EndFrame()
	{
		// 캐싱된 데이터들 해제
		ClearCache();

		// Present our back buffer to our front buffer
		m_pSwapChain->Present(m_rSyncInterval, 0);
	}

	uint32_t RenderManager::AddRenderer(Renderer* _renderer)
	{
		if (_renderer == nullptr)
			return UINT32_MAX;

		uint32_t id = m_nextRendererId++;
		m_renderers.push_back(_renderer);
		m_rendererIdMap[id] = _renderer;
		return id;
	}

	void RenderManager::RemoveRenderer(int _idx)
	{
		if (m_renderers.empty())
			return;

		auto it = m_rendererIdMap.find(static_cast<uint32_t>(_idx));
		if (it == m_rendererIdMap.end())
			return;

		Renderer* target = it->second;
		m_rendererIdMap.erase(it);

		for (size_t i = 0; i < m_renderers.size(); ++i)
		{
			if (m_renderers[i] == target)
			{
				m_renderers[i] = m_renderers.back();
				m_renderers.pop_back();
				break;
			}
		}
	}

	int RenderManager::AddLight(Light* _obj)
	{
		if (_obj == nullptr)
			return -1;

		int index = static_cast<int>(m_lights.size());
		m_lights.push_back(_obj);

		return index;
	}

	void RenderManager::RemoveLight(int _idx)
	{
		if (m_lights.empty())
			return;

		if (_idx < m_lights.size() && _idx >= 0)
		{
			if (m_lights.size() == 1)
			{
				m_lights.pop_back();
				return;
			}

			std::swap(m_lights[_idx], m_lights.back());
			m_lights[_idx]->m_lightIndex = _idx;
			m_lights.pop_back();
		}
	}

	void RenderManager::SetShadowMapSize(UINT _size)
	{

	}

	Renderer* RenderManager::GetRendererById(uint32_t id) const
	{
		auto it = m_rendererIdMap.find(id);
		if (it == m_rendererIdMap.end())
			return nullptr;
		return it->second;
	}

	void RenderManager::RegisterCanvas(Canvas* canvas)
	{
		if (!canvas)
			return;

		auto it = std::find(m_canvases.begin(), m_canvases.end(), canvas);
		if (it != m_canvases.end())
			return;

		m_canvases.push_back(canvas);
	}

	void RenderManager::UnRegisterCanvas(Canvas* canvas)
	{
		auto it = std::find(m_canvases.begin(), m_canvases.end(), canvas);
		if (it == m_canvases.end())
			return;

		*it = m_canvases.back();
		m_canvases.pop_back();
	}

	void RenderManager::BeginCanvas(Canvas* canvas)
	{
	}

	void RenderManager::EndCanvas()
	{
	}

	void RenderManager::SetUIStencilDisabled()
	{
		m_uiActiveDepthState = m_pUIDepthState.Get();
		m_uiStencilRef = 0;
		if (m_pDeviceContext)
			m_pDeviceContext->OMSetDepthStencilState(m_uiActiveDepthState, m_uiStencilRef);
	}

	void RenderManager::SetUIStencilTest(UINT stencilRef)
	{
		if (stencilRef > 0xFF)
			stencilRef = 0xFF;
		m_uiActiveDepthState = m_pUIStencilTestState.Get();
		m_uiStencilRef = stencilRef;
		if (m_pDeviceContext)
			m_pDeviceContext->OMSetDepthStencilState(m_uiActiveDepthState, m_uiStencilRef);
	}

	void RenderManager::SetUIStencilWriteIncrement(UINT stencilRef)
	{
		if (stencilRef > 0xFF)
			stencilRef = 0xFF;
		m_uiActiveDepthState = m_pUIStencilWriteState.Get();
		m_uiStencilRef = stencilRef;
		if (m_pDeviceContext)
			m_pDeviceContext->OMSetDepthStencilState(m_uiActiveDepthState, m_uiStencilRef);
	}

	void RenderManager::SetUIStencilWriteDecrement(UINT stencilRef)
	{
		if (stencilRef > 0xFF)
			stencilRef = 0xFF;
		m_uiActiveDepthState = m_pUIStencilClearState.Get();
		m_uiStencilRef = stencilRef;
		if (m_pDeviceContext)
			m_pDeviceContext->OMSetDepthStencilState(m_uiActiveDepthState, m_uiStencilRef);
	}

	void RenderManager::SetUIColorWriteEnabled(bool enabled)
	{
		if (!m_pDeviceContext)
			return;

		ID3D11BlendState* state = enabled ? m_pUIBlendState.Get() : m_pUIBlendStateNoColor.Get();
		if (!state)
			state = m_pUIBlendState.Get();

		float blendFactor[4] = { 0,0,0,0 };
		m_pDeviceContext->OMSetBlendState(state, blendFactor, 0xffffffff);
	}

	void RenderManager::SetUIMaskParams(bool enabled, float alphaThreshold)
	{
		m_uiMaskEnabled = enabled;
		if (alphaThreshold < 0.0f)
			alphaThreshold = 0.0f;
		if (alphaThreshold > 1.0f)
			alphaThreshold = 1.0f;
		m_uiMaskAlphaThreshold = alphaThreshold;
	}

	void RenderManager::DrawUIElement(const Vector4& rect, const Vector4& uvRect,
		const Color& color, const ResPtr<Texture2D>& texture,
		const Vector2& pivot, const Vector2& rightDir, const Vector2& upDir)
	{
		if (!m_pUIBuffer || m_sceneWidth == 0 || m_sceneHeight == 0)
			return;

		Render_UIBuffer data = {};
		data.rect = rect;
		data.uvRect = uvRect;
		data.color = color;
		data.screenParams = Vector4(
			static_cast<float>(m_sceneWidth),
			static_cast<float>(m_sceneHeight),
			texture ? 1.0f : 0.0f,
			0.0f);
		data.maskParams = Vector4(
			m_uiMaskEnabled ? 1.0f : 0.0f,
			m_uiMaskAlphaThreshold,
			0.0f,
			0.0f);
		data.transformParams0 = Vector4(
			pivot.x,
			pivot.y,
			rightDir.x,
			rightDir.y);
		data.transformParams1 = Vector4(
			upDir.x,
			upDir.y,
			0.0f,
			0.0f);
		data.viewProj = Matrix::Identity.Transpose();

		m_pDeviceContext->UpdateSubresource1(m_pUIBuffer.Get(), 0, nullptr, &data, 0, 0, D3D11_COPY_DISCARD);

		ID3D11ShaderResourceView* srv = texture ? texture->m_pSRV.Get() : nullptr;
		m_pDeviceContext->PSSetShaderResources(0, 1, &srv);
		m_pDeviceContext->Draw(6, 0);
	}

	void RenderManager::DrawUIText(const Vector4& rect,
		const std::wstring& text,
		const ResPtr<Font>& font,
		const Color& color,
		TextAlignment alignment,
		TextWrapMode wrapMode,
		float rotationRad,
		const Vector2& pivotScene,
		const Vector2& textScale)
	{
		if (text.empty() || !font)
			return;
		if (!m_pDeviceContext)
			return;

		EnsureUISpriteBatch();
		auto* spriteFont = GetSpriteFont(font);
		if (!m_uiSpriteBatch || !spriteFont)
			return;

		m_uiSpriteBatch->SetRotation(DXGI_MODE_ROTATION_IDENTITY);

		const DirectX::XMMATRIX transform = DirectX::XMMatrixIdentity();

		ID3D11RasterizerState* uiRs = m_pUIRS ? m_pUIRS.Get() : m_pDefaultRS.Get();
		ID3D11DepthStencilState* depthState = m_uiActiveDepthState ? m_uiActiveDepthState : m_pUIDepthState.Get();
		m_uiSpriteBatch->Begin(DirectX::SpriteSortMode_Deferred,
			m_pUIBlendState.Get(),
			m_pLinearSampler.Get(),
			depthState,
			uiRs,
			nullptr,
			transform);
		if (m_pDeviceContext && depthState && m_uiStencilRef != 0)
			m_pDeviceContext->OMSetDepthStencilState(depthState, m_uiStencilRef);

		auto endSpriteBatch = [this]()
			{
				m_uiSpriteBatch->End();

				// Restore UI pipeline for subsequent UI elements.
				EnsureUIShaders();
				if (!m_pUIVShader || !m_pUIPShader || !m_pUIBuffer)
					return;

				auto context = m_pDeviceContext.Get();
				context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
				context->IASetInputLayout(nullptr);
				context->IASetVertexBuffers(0, 0, nullptr, nullptr, nullptr);
				context->IASetIndexBuffer(nullptr, DXGI_FORMAT_R32_UINT, 0);
				context->VSSetShader(m_pUIVShader->m_pVShader.Get(), nullptr, 0);
				context->PSSetShader(m_pUIPShader->m_pPShader.Get(), nullptr, 0);
				context->VSSetConstantBuffers(0, 1, m_pUIBuffer.GetAddressOf());
				context->PSSetConstantBuffers(0, 1, m_pUIBuffer.GetAddressOf());
				context->PSSetSamplers(0, 1, m_pLinearSampler.GetAddressOf());

				float blendFactor[4] = { 0,0,0,0 };
				context->OMSetBlendState(m_pUIBlendState.Get(), blendFactor, 0xffffffff);
				ID3D11DepthStencilState* restoreDepth = m_uiActiveDepthState ? m_uiActiveDepthState : m_pUIDepthState.Get();
				context->OMSetDepthStencilState(restoreDepth, m_uiStencilRef);
				context->RSSetState(m_pUIRS ? m_pUIRS.Get() : m_pDefaultRS.Get());
			};

		DirectX::XMVECTOR sizeVec = DirectX::XMVectorZero();
		std::wstring renderText = text;
		try
		{
			sizeVec = spriteFont->MeasureString(renderText.c_str(), false);
		}
		catch (const std::exception&)
		{
			renderText = FilterTextForSpriteFont(*spriteFont, renderText);
			if (renderText.empty())
			{
				endSpriteBatch();
				return;
			}

			try
			{
				sizeVec = spriteFont->MeasureString(renderText.c_str(), false);
			}
			catch (const std::exception&)
			{
				endSpriteBatch();
				return;
			}
		}

		float scaleX = std::abs(textScale.x);
		float scaleY = std::abs(textScale.y);
		if (scaleX <= 1e-6f) scaleX = 1.0f;
		if (scaleY <= 1e-6f) scaleY = 1.0f;

		const float unscaledWrapWidth = rect.z / scaleX;
		if (wrapMode != TextWrapMode::NoWrap && unscaledWrapWidth > 1e-6f)
		{
			try
			{
				renderText = WrapTextToWidth(*spriteFont, renderText, wrapMode, unscaledWrapWidth);
			}
			catch (const std::exception&)
			{
				endSpriteBatch();
				return;
			}
		}

		try
		{
			sizeVec = spriteFont->MeasureString(renderText.c_str(), false);
		}
		catch (const std::exception&)
		{
			endSpriteBatch();
			return;
		}

		const float textWidth = DirectX::XMVectorGetX(sizeVec) * scaleX;

		float x = rect.x;
		if (alignment == TextAlignment::Center)
			x += (rect.z - textWidth) * 0.5f;
		else if (alignment == TextAlignment::Right)
			x += (rect.z - textWidth);

		const float y = rect.y;
		try
		{
			const DirectX::XMFLOAT2 topLeft(x, y);
			const DirectX::XMFLOAT2 pos(pivotScene.x, pivotScene.y);
			const float invScaleX = 1.0f / scaleX;
			const float invScaleY = 1.0f / scaleY;
			const DirectX::XMFLOAT2 origin(
				(pivotScene.x - topLeft.x) * invScaleX,
				(pivotScene.y - topLeft.y) * invScaleY);
			spriteFont->DrawString(m_uiSpriteBatch.get(), renderText.c_str(),
				pos, color, rotationRad, origin, DirectX::XMFLOAT2(scaleX, scaleY));
		}
		catch (const std::exception&)
		{
			endSpriteBatch();
			return;
		}

		endSpriteBatch();
	}
}
