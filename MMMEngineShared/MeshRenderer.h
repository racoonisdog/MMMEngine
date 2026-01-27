#pragma once

#include "Export.h"
#include "ResourceManager.h"
#include "Renderer.h"
#include "rttr/type"

namespace MMMEngine {
	class StaticMesh;
	class MMMENGINE_API MeshRenderer : public Renderer {
		RTTR_ENABLE(Renderer)
		RTTR_REGISTRATION_FRIEND
	private:
		// GPU ¹öÆÛ
		ResPtr<StaticMesh> mesh = nullptr;

		void Initialize() override;
		void UnInitialize() override;
		void Init() override;
		void Render() override;
	public:
		ResPtr<StaticMesh>& GetMesh() { return mesh; }
		void SetMesh(ResPtr<StaticMesh>& _mesh);
	};
}

