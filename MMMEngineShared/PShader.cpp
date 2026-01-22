#include "PShader.h"
#include "RenderManager.h"
#include "RendererTools.h"
#include <d3dcompiler.h>

namespace fs = std::filesystem;

bool MMMEngine::PShader::LoadFromFilePath(const std::wstring& filePath)
{
	if (!fs::exists(fs::path(filePath)))
		throw std::runtime_error("PShader::File not exist !!");

	auto m_pDevice = RenderManager::Get().GetDevice();

	// PS쉐이더 컴파일
	Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

	HR_T(D3DCompileFromFile(
		filePath.c_str(),
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"main",
		"ps_5_0",
		0, 0,
		m_pBlob.GetAddressOf(),
		errorBlob.GetAddressOf()));

	HR_T(m_pDevice->CreatePixelShader(
		m_pBlob->GetBufferPointer(),
		m_pBlob->GetBufferSize(),
		nullptr,
		m_pPShader.GetAddressOf()));

	return true;
}
