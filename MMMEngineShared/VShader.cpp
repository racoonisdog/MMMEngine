#include "VShader.h"
#include "RenderManager.h"
#include "RendererTools.h"
#include <d3dcompiler.h>

namespace fs = std::filesystem;

bool MMMEngine::VShader::LoadFromFilePath(const std::wstring& filePath)
{
	if (!fs::exists(fs::path(filePath)))
		throw std::runtime_error("VSShader::File not exist !!");

	auto m_pDevice = RenderManager::Get().GetDevice();

	// VS쉐이더 컴파일
	Microsoft::WRL::ComPtr<ID3D10Blob> errorBlob;

	HR_T(D3DCompileFromFile(
		filePath.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"vs_5_0",
		0, 0,
		m_pBlob.GetAddressOf(),
		errorBlob.GetAddressOf()));

	HR_T(m_pDevice->CreateVertexShader(
		m_pBlob->GetBufferPointer(),
		m_pBlob->GetBufferSize(),
		nullptr,
		m_pVShader.GetAddressOf()));

	// 기본 InputLayout 생성
	// TODO :: 하드코딩으로 생성하지말고 리플렉션 사용하는방법 알아보기
	D3D11_INPUT_ELEMENT_DESC layout[] = {
   { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
   { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
   { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
   { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
   { "BONEINDEX", 0, DXGI_FORMAT_R32G32B32A32_SINT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 },
   { "BONEWEIGHT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_VERTEX_DATA, 0 }
	};
	HR_T(m_pDevice->CreateInputLayout(
		layout, ARRAYSIZE(layout), m_pBlob->GetBufferPointer(),
		m_pBlob->GetBufferSize(), m_pInputLayout.GetAddressOf()
	));

	return true;
}
