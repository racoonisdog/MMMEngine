#pragma once

#include "Export.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "rttr/type"

namespace MMMEngine {
	class StaticMesh;
	class Material;
	class MMMENGINE_API MeshRenderer : public Renderer {
		RTTR_ENABLE(Renderer)
		RTTR_REGISTRATION_FRIEND
	private:
		// GPU 버퍼
		ResPtr<StaticMesh> mesh = nullptr;

		void Initialize() override;
		void UnInitialize() override;
		void Render() override;
	public:
		ResPtr<StaticMesh> GetMesh() { return mesh; }
		void SetMesh(ResPtr<StaticMesh> _mesh);
		float ditherAlpha = 1.0f;

		void SetCastShadow(bool _val);
		bool GetCastShadow();
		void SetReceiveShadow(bool _val);
		bool GetReceiveShadow();

		void SetMaterial(std::vector<ResPtr<Material>>& _materials);
		std::vector<ResPtr<Material>>& GetMaterial();

		float GetDitherAlpha() const { return ditherAlpha; }
		void SetDitherAlpha(float _alpha) { ditherAlpha = (_alpha <= 0.0f) ? 0.0f : ((_alpha >= 1.0f) ? 1.0f : _alpha); }
	};
}

