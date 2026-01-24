#pragma once
#include "Export.h"
#include <memory>
#include <unordered_map>
#include <type_traits>
#include <typeindex>
#include <filesystem>

#include <vector>

#include "MUID.h"
#include "ExportSingleton.hpp"

#include "Resource.h"
// todo 삭제
#include <iostream>

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
		std::filesystem::path m_rootPath;
	public:
		std::filesystem::path GetCurrentRootPath() { return m_rootPath; }

		void ClearCache() { m_cache.clear(); }

		void StartUp(std::wstring rootPath)
		{
			m_rootPath = rootPath;
		}

		void ShutDown()
		{
			m_cache.clear();
		}

		template<class T>
		ResPtr<T> Load(std::wstring filePath)
		{
			static_assert(std::is_base_of_v<Resource, T>, "T는 반드시 Resource를 상속받아야 합니다.");

			std::wstring root = m_rootPath.generic_wstring();
			if (!root.empty() && root.back() != L'/' && root.back() != L'\\')
				root += L'/';

			std::wstring truePath = m_rootPath.generic_wstring() + filePath;

			ResKey key{ rttr::type::get<T>().get_name().to_string(), truePath };

			if (auto it = m_cache.find(key); it != m_cache.end())
				if (auto sp = it->second.lock())
					return std::dynamic_pointer_cast<T>(sp);

			auto res = std::make_shared<T>();
			res->SetFilePath(filePath);
			if (!res->LoadFromFilePath(truePath))
			{
				std::cout << u8"파일 패스가 잘못되었어용" << std::endl;
				return nullptr;
			}

			m_cache[key] = res;
			return res;
		}

		// RTTR용 Load 함수 (SceneSerializer에서 사용)
		rttr::variant Load(rttr::type resourceType, const std::wstring& filePath)
		{
			if (!resourceType.is_valid())
				return rttr::variant();

			std::wstring root = m_rootPath.generic_wstring();
			if (!root.empty() && root.back() != L'/' && root.back() != L'\\')
				root += L'/';

			std::wstring truePath = m_rootPath.generic_wstring() + filePath;

			std::string typeName = resourceType.get_name().to_string();
			ResKey key{ typeName, truePath };

			// 캐시 확인
			auto it = m_cache.find(key);
			if (it != m_cache.end())
			{
				if (auto sp = it->second.lock())
					return rttr::variant(sp);

				// 만료된 weak_ptr 제거
				m_cache.erase(it);
			}

			// 새로 생성
			rttr::variant resource = resourceType.create();
			if (!resource.is_valid())
				return rttr::variant();

			// shared_ptr로 직접 얻기
			std::shared_ptr<Resource> resPtr = resource.get_value<std::shared_ptr<Resource>>();
			if (!resPtr)
				return rttr::variant();  // shared_ptr이 아니면 실패

			// 파일 로드
			resPtr->SetFilePath(filePath);
			if (!resPtr->LoadFromFilePath(truePath))
				return rttr::variant();

			m_cache[key] = resPtr;
			return rttr::variant(resPtr);
		}

		bool Contains(const std::string& typeString, const std::wstring& filePath)
		{
			std::wstring truePath = m_rootPath.generic_wstring() + filePath;
			ResKey key{ typeString, truePath };

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