#pragma once

#include "RendererBase.h"

class MMMENGINE_API SkyboxRenderer : public MMMEngine::RendererBase
{
private:
	Microsoft::WRL::ComPtr<ID3D11InputLayout> m_pInputLayout;	// 입력 레이아웃 (참조일수 있음)

public:

};

