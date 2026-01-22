#pragma once

#include "Export.h"
#include "Behaviour.h"
#include "ResourceManager.h"

namespace MMMEngine {
	class StaticMesh;
	class RendererBase;
	class MMMENGINE_API MeshRenderer : public Behaviour
	{
		RTTR_ENABLE(Behaviour)
			RTTR_REGISTRATION_FRIEND
			friend class BehaviourManager;
			friend class GameObject;
	private:
		// GPU ¹öÆÛ
		ResPtr<StaticMesh> mesh = nullptr;
		std::vector<std::weak_ptr<RendererBase>> renderers;
	public:
		MeshRenderer();
		~MeshRenderer();

		ResPtr<StaticMesh>& GetMesh() { return mesh; }
;		void SetMesh(ResPtr<StaticMesh>& _mesh);
		void Start();
		void Update();
	};
}

