#include "Texture2D.h"
#include <wrl/client.h>
#include <d3d11_4.h>
#include "RenderManager.h"
#include <DirectXTex.h>
#include <RendererTools.h>
#include <WICTextureLoader.h>
#include <DDSTextureLoader.h>

#include <rttr/registration>

namespace WRL = Microsoft::WRL;
namespace fs = std::filesystem;

RTTR_REGISTRATION
{
	using namespace rttr;
	using namespace MMMEngine;

	registration::class_<Texture2D>("Texture2D")
		.constructor<>()(policy::ctor::as_std_shared_ptr)
		.property_readonly("GetFilePath",&Texture2D::GetFilePath);

	type::register_converter_func(
		[](std::shared_ptr<Resource> from, bool& ok) -> std::shared_ptr<Texture2D>
		{
			if (!from) {  // nullptr 허용
				ok = true;
				return nullptr;
			}
			auto result = std::dynamic_pointer_cast<Texture2D>(from);
			ok = (result != nullptr);
			return result;
		}
	);
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
	else if (_path.extension() == L".dds") {
		ID3D11DeviceContext* context = nullptr;
		m_pDevice->GetImmediateContext(&context);

		HR_T(DirectX::CreateDDSTextureFromFile(
			m_pDevice.Get(),
			context,
			_path.wstring().c_str(),
			nullptr, // ID3D11Resource** (필요 없으면 nullptr)
			_out     // ID3D11ShaderResourceView** 반환
		));
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
	if (!fs::exists(fPath))
		throw std::runtime_error("Texture2D::File does not Exist !!!");

	WRL::ComPtr<ID3D11ShaderResourceView> srv;
	CreateResourceView(fPath, srv.GetAddressOf());

	if (srv) {
		srv.As(&m_pSRV);
		return true;
	}

	return false;
}
