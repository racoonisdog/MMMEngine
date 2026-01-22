#pragma once
#include "Export.h"
#include "Resource.h"
#include "rttr/type"

namespace MMMEngine {
	class MMMENGINE_API AnimationClip : public Resource
	{
		RTTR_ENABLE(Resource);
		RTTR_REGISTRATION_FRIEND
			friend class ResourceManager;
			friend class SceneManager;
			friend class Scene;
	public:


		//TODO::역직렬화시키기
		bool LoadFromFilePath(const std::wstring& filePath) override;
	};
}

