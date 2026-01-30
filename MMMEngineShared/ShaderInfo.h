#pragma once
#define NOMINMAX
#include "ExportSingleton.hpp"
#include <unordered_map>
#include <string>
#include <SimpleMath.h>
#include <wrl/client.h>
#include <d3d11_4.h>
#include <d3d11shader.h>
#include <variant>

#include "RendererTools.h"
#include "ResourceManager.h"
#include "RenderShared.h"

#include "VShader.h"
#include "PShader.h"
#include "Texture2D.h"

// 쉐이더 정보를 정의하는 헤더입니다
// 여기서 쉐이더 정보를 하드코딩해야합니다.
namespace MMMEngine {
	using PropertyValue = std::variant<
		int, float, DirectX::SimpleMath::Vector3, DirectX::SimpleMath::Vector4, DirectX::SimpleMath::Matrix,
		ResPtr<MMMEngine::Texture2D>
	>;

	// 개별로 정의할 쉐이더 타입
	enum ShaderType : int {
		S_PBR = 0,
		S_TOON = 1,
		S_PHONG = 2,
		S_SKYBOX = 3,
	};

	// -- 상수 정보 만들고 스타트업, json에 같은양식으로 등록할것!! --
	struct Render_LightBuffer {
		DirectX::SimpleMath::Vector3 mLightDir = { .0f, .0f, 1.0f };
		float mLightPadding = 0.0f;
		DirectX::SimpleMath::Vector3 mLightColor = { 1.0f, .0f, 1.0f };
		float mIntensity = 1.0f;
	};

	struct Render_ShadowBuffer
	{
		DirectX::SimpleMath::Matrix ShadowView;
		DirectX::SimpleMath::Matrix ShadowProjection;
	};

	struct PBR_MaterialBuffer
	{
		DirectX::SimpleMath::Color mBaseColorFactor;

		float mMetalicFactor = 0.0f;
		float mRoughnessFactor = 0.0f;
		float mAoFactor = 1.0f;
		float mEmissiveFactor = 1.0f;
	};
	// ----

	enum class PropertyType : int {
		Texture = 0,
		Constant = 1,
		Sampler = 2,
	};

	struct TypeInfo {
		ShaderType shaderType = ShaderType::S_PBR;		// 쉐이더타입
		RenderType renderType = RenderType::R_GEOMETRY;	// 렌더타입
	};

	struct PropertyInfo {
		PropertyType propertyType = PropertyType::Texture;
		int bufferIndex = -1;	// 버퍼번호

		D3D_SHADER_VARIABLE_TYPE varType;
		UINT rows;    // 행 수 (예: float4 → 1)
		UINT columns; // 열 수 (예: float4 → 4)
	};

	struct CBPropertyInfo {
		std::wstring bufferName;	// 상수버퍼 이름
		UINT offset;	// 상수버퍼 내에서 해당 변수의 시작 위치(바이트 단위)
		UINT size;		// 변수의 크기(바이트 단위)
	};

	struct BufferInfo {
		std::wstring bufferName;
		int registerIndex = -1;
	};

	class RenderManager;
	class MMMENGINE_API ShaderInfo : public Utility::ExportSingleton<ShaderInfo>
	{
		friend class RenderManager;
	private: 
		// 기본값 쉐이더
		ResPtr<VShader> m_pDefaultVShader;
		ResPtr<PShader> m_pDefaultPShader;

		// 쉐이더 타입정보정의 < ShaderPath, TypeInfo >
		std::unordered_map<std::wstring, TypeInfo> m_typeInfoMap;

		// 쉐이더 타입별 사용버퍼 < ShaderType, < BufferName, RegisterIdx>>
		std::unordered_map<ShaderType, std::vector<BufferInfo>> m_typeBufferMap;

		// 쉐이더타입별 메테리얼 프로퍼티 정의 < ShaderType, <PropertyName, PropertyInfo>>
		std::unordered_map<ShaderType, std::unordered_map<std::wstring, PropertyInfo>> m_propertyInfoMap;

		// 상수버퍼 타입정의 맵 <ShaderType, <propertyName, CBPropertyInfo>
		std::unordered_map<ShaderType, std::unordered_map<std::wstring, CBPropertyInfo>> m_CBPropertyMap;

		// 상수버퍼 저장용 맵 <constantBufferName, ID3D11Buffer>
		std::unordered_map<std::wstring, Microsoft::WRL::ComPtr<ID3D11Buffer>> m_CBBufferMap;

		// 글로벌 리소스 저장용 맵 <ShaderType, <propertyName ,PropertyValue>>
		std::unordered_map<ShaderType, std::unordered_map<std::wstring, PropertyValue>> m_globalPropMap;

		//// 텍스쳐 버퍼인덱스 주는 맵 <propertyName, index> (int == shader tN)
		//std::unordered_map<ShaderType, std::unordered_map<std::wstring, int>> m_texPropertyMap;
		
		
		void CreatePShaderReflection(std::wstring&& _filePath);

		template<typename T>
		Microsoft::WRL::ComPtr<ID3D11Buffer> CreateConstantBuffer();

		void DeSerialize();
	public:
		void StartUp();
		void ShutDown();

	public:
		std::wstring GetDefaultVShader();
		std::wstring GetDefaultPShader();

		const RenderType GetRenderType(const std::wstring& _shaderPath);
		const ShaderType GetShaderType(const std::wstring& _shaderPath);
		const int PropertyToIdx(const ShaderType _type, const std::wstring& _propertyName, PropertyInfo* _out = nullptr) const;
		void MMMEngine::ShaderInfo::UpdateProperty(ID3D11DeviceContext4* context,
			const ShaderType shaderType,
			const std::wstring& propertyName,
			const void* data);
		void UpdateCBuffers(const ShaderType _type);

		void ConvertMaterialType(const ShaderType _type, Material* _mat);

		void AddGlobalPropVal(const ShaderType _type, const std::wstring _propName, const PropertyValue& _value);
		void SetGlobalPropVal(const ShaderType _type, const std::wstring _propName, const PropertyValue& _value);
		void RemoveGlobalPropVal(const ShaderType _type, const std::wstring _propName);

		Microsoft::WRL::ComPtr<ID3D11InputLayout> CreateVShaderLayout(ID3D10Blob* _blob);
	};

	template<typename T>
	Microsoft::WRL::ComPtr<ID3D11Buffer> MMMEngine::ShaderInfo::CreateConstantBuffer()
	{
		D3D11_BUFFER_DESC bd = {};
		bd.Usage = D3D11_USAGE_DYNAMIC;
		bd.ByteWidth = sizeof(T);						// 구조체 크기
		bd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;		// 상수버퍼로 바인딩
		bd.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;		// CPU 접근 필요

		Microsoft::WRL::ComPtr<ID3D11Buffer> buffer;
		HR_T(RenderManager::Get().GetDevice()->CreateBuffer(&bd, nullptr, buffer.GetAddressOf()));
		return buffer;
	}
}



