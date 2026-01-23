#pragma once
#include "Export.h"
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <typeindex>

#include <vector>

#include "MUID.h"
#include "ExportSingleton.hpp"

#include "Resource.h"

namespace MMMEngine
{
	template <typename T>
	using ResPtr = std::shared_ptr<T>;

	template <typename T>
	using ResWeakPtr = std::weak_ptr<T>;

	struct ResKey
	{
		std::string typeName;
		std::wstring path;

		bool operator==(const ResKey& o) const noexcept {
			return typeName == o.typeName && path == o.path;
		}
	};

	struct ResKeyHash
	{
		size_t operator()(const ResKey& k) const noexcept
		{
			size_t h1 = std::hash<std::wstring>{}(k.path);
			size_t h2 = std::hash<std::string>{}(k.typeName);
			return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
		}
	};

	class MMMENGINE_API ResourceManager : public Utility::ExportSingleton<ResourceManager>
	{
	private:
		std::unordered_map<ResKey, std::weak_ptr<Resource>, ResKeyHash> m_cache;

	public:
		void ClearCache() { m_cache.clear(); }

		template<class T>
		ResPtr<T> Load(std::wstring filePath)
		{
			static_assert(std::is_base_of_v<Resource, T>, "T는 반드시 Resource를 상속받아야 합니다.");

			ResKey key{ rttr::type::get<T>().get_name().to_string(), filePath };

			if (auto it = m_cache.find(key); it != m_cache.end())
				if (auto sp = it->second.lock())
					return std::dynamic_pointer_cast<T>(sp);

			auto res = std::make_shared<T>();
			res->SetFilePath(filePath);
			if (!res->LoadFromFilePath(filePath))
				return nullptr;

			m_cache[key] = res;
			return res;
		}

		// RTTR용 Load 함수 (SceneSerializer에서 사용)
		rttr::variant Load(rttr::type resourceType, const std::wstring& filePath)
		{
			if (!resourceType.is_valid())
				return rttr::variant();

			std::string typeName = resourceType.get_name().to_string();
			ResKey key{ typeName, filePath };

			// 캐시 확인
			if (auto it = m_cache.find(key); it != m_cache.end())
				if (auto sp = it->second.lock())
				{
					// ResPtr<T> 타입으로 variant 생성
					rttr::type resPtrType = rttr::type::get_by_name("ResPtr<" + typeName + ">");
					if (resPtrType.is_valid())
					{
						rttr::variant result = resPtrType.create();
						// shared_ptr을 variant에 저장
						result = std::dynamic_pointer_cast<Resource>(sp);
						return result;
					}
				}

			// 새로 생성
			rttr::variant resource = resourceType.create();
			if (!resource.is_valid())
				return rttr::variant();

			Resource* rawPtr = resource.get_value<Resource*>();
			if (!rawPtr)
			{
				// shared_ptr로 시도
				auto sharedRes = resource.get_value<std::shared_ptr<Resource>>();
				if (sharedRes)
					rawPtr = sharedRes.get();
			}

			if (!rawPtr)
				return rttr::variant();

			rawPtr->SetFilePath(filePath);
			if (!rawPtr->LoadFromFilePath(filePath))
				return rttr::variant();

			// shared_ptr 생성 및 캐싱
			std::shared_ptr<Resource> resPtr;
			if (auto sharedRes = resource.get_value<std::shared_ptr<Resource>>())
			{
				resPtr = sharedRes;
			}
			else
			{
				// 원시 포인터인 경우 shared_ptr로 변환 (주의: 소유권 관리 필요)
				resPtr = std::shared_ptr<Resource>(rawPtr);
			}

			m_cache[key] = resPtr;

			// ResPtr<T> 형태로 반환
			return rttr::variant(resPtr);
		}

		bool Contains(const std::string& typeString, const std::wstring& filePath)
		{
			ResKey key{ typeString, filePath };

			auto resource_iter = m_cache.find(key);
			if (resource_iter != m_cache.end())
			{
				auto res_shared = resource_iter->second.lock();
				if (res_shared)
					return true;

				m_cache.erase(resource_iter);
			}

			return false;
		}
	};
}