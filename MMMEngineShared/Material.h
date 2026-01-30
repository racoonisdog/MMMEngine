#pragma once

#include "Export.h"
#include <SimpleMath.h>
#include <wrl/client.h>
#include <d3d11_4.h>

#include <filesystem>

#include "ResourceManager.h"
#include "Texture2D.h"

#include "ShaderInfo.h"
#include "json/json.hpp"
#include "rttr/type"

namespace MMMEngine {
	class PShader;
	class VShader;
	class MaterialSerializer;
	class RenderManager;
	class MMMENGINE_API Material : public Resource
	{
		friend class MaterialSerializer;
		friend class RenderManager;
		friend class ShaderInfo;
		RTTR_ENABLE(Resource);
		RTTR_REGISTRATION_FRIEND
			friend class ResourceManager;
			friend class SceneManager;
			friend class Scene;
	private:
		std::unordered_map<std::wstring, PropertyValue> m_properties;
		ResPtr<VShader> m_pVShader;
		ResPtr<PShader> m_pPShader;

	public:
		void AddProperty(const std::wstring _name, const PropertyValue& _value);
		void SetProperty(const std::wstring _name, const PropertyValue& _value);
		void RemoveProperty(const std::wstring& _name);
		PropertyValue GetProperty(const std::wstring& name) const;
		const std::unordered_map<std::wstring, PropertyValue>& GetProperties() { return m_properties; }

		void SetVShader(ResPtr<VShader> _vShader);
		void SetPShader(ResPtr<PShader> _pShader);
		ResPtr<VShader> GetVShader();
		ResPtr<PShader> GetPShader();

		void LoadTexture(const std::wstring& _name, const std::wstring& _filePath);

		bool LoadFromFilePath(const std::wstring& _filePath) override;

		bool operator<(const Material& other) const {
			return GetFilePath() < other.GetFilePath(); // Resource에 이름이 있다고 가정
		}

		bool operator==(const Material& other) const {
			return GetFilePath() == other.GetFilePath();
		}
	};
}


