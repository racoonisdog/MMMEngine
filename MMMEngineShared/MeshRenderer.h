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
		// GPU 버퍼
		ResPtr<StaticMesh> mesh = nullptr;
		// 메테리얼 path맵
		std::unordered_map<int, std::wstring> m_shaderPathMap;
	public:
		MeshRenderer();
		 
		ResPtr<StaticMesh>& GetMesh() { return mesh; }
;		void SetMesh(ResPtr<StaticMesh>& _mesh);
		void Start();
		void Update();
	};
}

