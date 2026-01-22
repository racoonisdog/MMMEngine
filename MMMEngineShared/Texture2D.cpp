#include "Texture2D.h"
#include <wrl/client.h>
#include <d3d11_4.h>
#include "RenderManager.h"
#include <DirectXTex.h>
#include <RendererTools.h>
#include <WICTextureLoader.h>

#include <rttr/registration>

namespace WRL = Microsoft::WRL;
namespace fs = std::filesystem;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Texture2D>("Texture2D")
		.property_readonly("GetFilePath",&Texture2D::GetFilePath);
}

// SRV 만들어주는 함수
void MMMEngine::Texture2D::CreateResourceView(std::filesystem::path& _path, ID3D11ShaderResourceView** _out)
{
	// 디바이스 가져오기
	auto m_pDevice = RenderManager::Get().GetDevice();

	// TGA파일 확인
	if (_path.extension() == L".tga") {
		DirectX::ScratchImage image;
		DirectX::TexMetadata meta;
		HR_T(DirectX::LoadFromTGAFile(_path.wstring().c_str(), &meta, image));
		HR_T(DirectX::CreateShaderResourceView(m_pDevice.Get(), image.GetImages(), image.GetImageCount(), meta, _out));
	}
	else {
		ID3D11DeviceContext* context = nullptr;
		m_pDevice->GetImmediateContext(&context);

		HR_T(CreateWICTextureFromFileEx(
			m_pDevice.Get(),
			context,
			_path.wstring().c_str(),
			0,
			D3D11_USAGE_DEFAULT,
			D3D11_BIND_SHADER_RESOURCE,
			0, 0,
			DirectX::DX11::WIC_LOADER_FLAGS::WIC_LOADER_FORCE_RGBA32 | DirectX::DX11::WIC_LOADER_FLAGS::WIC_LOADER_IGNORE_SRGB,
			nullptr,
			_out));
	}
}

bool MMMEngine::Texture2D::LoadFromFilePath(const std::wstring& filePath)
{
	fs::path fPath(filePath);

	WRL::ComPtr<ID3D11ShaderResourceView> srv;
	CreateResourceView(fPath, srv.GetAddressOf());

	if (srv) {
		srv.As(&m_pSRV);
		return true;
	}

	return false;
}
