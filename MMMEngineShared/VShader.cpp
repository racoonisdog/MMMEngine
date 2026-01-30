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

	// InputLayout 생성
	m_pInputLayout = ShaderInfo::Get().CreateVShaderLayout(m_pBlob.Get());

	return true;
}
