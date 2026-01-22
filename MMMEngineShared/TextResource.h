#include "Resource.h"

namespace MMMEngine
{
	class MMMENGINE_API TextResource final : public Resource
	{
	private:
		RTTR_ENABLE(Resource)
		RTTR_REGISTRATION_FRIEND
		friend class SceneSerializer;

		std::vector<uint8_t> m_buffer;

		virtual bool LoadFromFilePath(const std::wstring& filePath);
	public:
		TextResource() = default;
		virtual ~TextResource() = default;

		const std::vector<uint8_t>& GetBuffer();
	};
}