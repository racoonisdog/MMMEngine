#define NOMINMAX
#pragma once
#include "ExportSingleton.hpp"
#include <unordered_map>
#include <string>
#include <SimpleMath.h>
#include <wrl/client.h>
#include <d3d11_4.h>
#include "RendererTools.h"
#include "RenderManager.h"
#include <d3d11shader.h>

// 쉐이더 정보를 정의하는 헤더입니다
// 여기서 쉐이더 정보를 하드코딩해야합니다.
namespace MMMEngine {

	// 개별로 정의할 쉐이더 타입
	enum ShaderType : int {
		S_PBR = 0,
		S_TOON = 1,
		S_PHONG = 2,
		S_SKYBOX = 3,
	};

	// -- 상수 정보는 만들고 cpp 생성자에 같은양식으로 등록할것!! --
	struct Render_LightBuffer {
		DirectX::SimpleMath::Vector4 mLightDir;
		DirectX::SimpleMath::Vector4 mLightColor;
	};

	struct Render_ShadowBuffer
	{
		DirectX::SimpleMath::Matrix ShadowView;
		DirectX::SimpleMath::Matrix ShadowProjection;
	};

	struct PBR_MaterialBuffer
	{
		DirectX::SimpleMath::Color mBaseColorFactor;

		float mMetalicFactor = 1.0f;
		float mRoughnessFactor = 1.0f;
		float mAoFactor = 1.0f;
		float mEmissiveFactor = 1.0f;
	};
	// ----

	enum class PropertyType : int {
		Texture = 0,
		Constant = 1,
	};

	class MMMENGINE_API ShaderInfo : public Utility::ExportSingleton<ShaderInfo>
	{
	private:
		// 쉐이더타입별 프로퍼티 정의 <PropertyName, PropertyInfo>
		std::unordered_map<ShaderType, std::unordered_map<std::wstring, PropertyType>> m_typeInfo;
		// 텍스쳐 버퍼인덱스 주는 맵 <propertyName, index> (int == shader tN)
		std::unordered_map<ShaderType, std::unordered_map<std::wstring, int>> m_matPropertyMap;
		// 상수버퍼 타입정의 맵 <propertyName, constantBufferName> (wstring == m_CBMap Key)
		std::unordered_map<ShaderType, std::unordered_map<std::wstring, std::wstring>> m_matCBPropertyMap;
		// 상수버퍼 인덱스 주는 맵 <constantBufferName, index> (int == shader bN)
		std::unordered_map<ShaderType, std::unordered_map<std::wstring, int>> m_CBIndexMap;
		// 상수버퍼 저장용 맵 <constantBufferName, ID3D11Buffer>
		std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11Buffer>> m_CBBufferMap;

		// 리플렉션 저장용 맵 <ShaderType, Reflection>
		std::unordered_map<ShaderType, Microsoft::WRL::ComPtr<ID3D11ShaderReflection>> m_SRMap;

		
		
		Microsoft::WRL::ComPtr<ID3D11ShaderReflection> MakeShaderReflection(ID3DBlob* _byteCode);

		template<typename T>
		Microsoft::WRL::ComPtr<ID3D11Buffer> CreateConstantBuffer();

		void DeSerialize();
	public:
		void StartUp();
		void ShutDown();
		const int PropertyToIdx(const ShaderType _type, const std::wstring& _propertyName, PropertyType* _out = nullptr) const;
	};

	template<typename T>
	Microsoft::WRL::ComPtr<ID3D11Buffer> MMMEngine::ShaderInfo::CreateConstantBuffer()
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DEFAULT;
		bd.ByteWidth = sizeof(T);						// 구조체 크기
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;		// 상수버퍼로 바인딩
		bd.CPUAccessFlags = 0;							// CPU 접근 불필요

		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		HR_T(RenderManager::Get().GetDevice()->CreateBuffer(&bd, nullptr, buffer.GetAddressOf()));
		return buffer;
	}
}



