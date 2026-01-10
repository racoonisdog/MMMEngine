#pragma once
#include "fast_delegate/MultiCastDelegate.h"
#include <stdexcept>

namespace MMMEngine::Utility
{
	template<typename... Args>
	class Action {
	public:
		using MulticastType = SA::multicast_delegate<void(Args...)>;
		using DelegateType = SA::delegate<void(Args...)>;

		Action() = default;
		~Action() = default;

		template<void(*Function)(Args...)>
		void AddListener() {
			auto delegate = DelegateType::template create<Function>();

			// 중복 체크
			if (Contains(delegate))
				return;

			m_multicast += delegate;
		}

		template<typename T, void(T::* Method)(Args...)>
		void AddListener(T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (Contains(delegate))
				return;

			m_multicast += delegate;
		}

		template<typename T, void(T::* Method)(Args...) const>
		void AddListener(const T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (Contains(delegate))
				return;

			m_multicast += delegate;
		}

		template<typename Lambda>
		void AddListenerLambda(const Lambda& lambda) {
			auto delegate = DelegateType::template create<Lambda>(lambda);
			m_multicast += delegate;
		}

		template<void(*Function)(Args...)>
		bool RemoveListener() {
			auto delegate = DelegateType::template create<Function>();

			if (!Contains(delegate))
				return false;

			m_multicast -= delegate;
			return true;
		}

		template<typename T, void(T::* Method)(Args...)>
		bool RemoveListener(T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (!Contains(delegate))
				return false;

			m_multicast -= delegate;
			return true;
		}

		template<typename T, void(T::* Method)(Args...) const>
		bool RemoveListener(const T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (!Contains(delegate))
				return false;

			m_multicast -= delegate;
			return true;
		}

		void Clear() {
			m_multicast.clear();
		}

		size_t Count() const {
			return m_multicast.size();
		}

		void Invoke(Args... args) const {
			m_multicast(args...);
		}

		void operator()(Args... args) const {
			Invoke(args...);
		}

	private:
		MulticastType m_multicast;

		bool Contains(const DelegateType& delegate) const {
			return m_multicast == delegate && m_multicast.size() == 1;
		}
	};

	template<typename R, typename... Args>
	class Func {
	public:
		using MulticastType = SA::multicast_delegate<R(Args...)>;
		using DelegateType = SA::delegate<R(Args...)>;

		Func() = default;
		~Func() = default;

		template<R(*Function)(Args...)>
		void AddListener() {
			auto delegate = DelegateType::template create<Function>();

			if (Contains(delegate))
				return;

			m_multicast += delegate;
		}

		template<typename T, R(T::* Method)(Args...)>
		void AddListener(T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (Contains(delegate))
				return;

			m_multicast += delegate;
		}

		template<typename T, R(T::* Method)(Args...) const>
		void AddListener(const T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (Contains(delegate))
				return;

			m_multicast += delegate;
		}

		template<typename Lambda>
		void AddListenerLambda(const Lambda& lambda) {
			auto delegate = DelegateType::template create<Lambda>(lambda);
			m_multicast += delegate;
		}

		template<R(*Function)(Args...)>
		bool RemoveListener() {
			auto delegate = DelegateType::template create<Function>();

			if (!Contains(delegate))
				return false;

			m_multicast -= delegate;
			return true;
		}

		template<typename T, R(T::* Method)(Args...)>
		bool RemoveListener(T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (!Contains(delegate))
				return false;

			m_multicast -= delegate;
			return true;
		}

		template<typename T, R(T::* Method)(Args...) const>
		bool RemoveListener(const T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (!Contains(delegate))
				return false;

			m_multicast -= delegate;
			return true;
		}

		void Clear() {
			m_multicast.clear();
		}

		size_t Count() const {
			return m_multicast.size();
		}

		R Invoke(Args... args) const {
			return m_multicast(args...);
		}

		R operator()(Args... args) const {
			return Invoke(args...);
		}

		template<typename Handler>
		void InvokeWithHandler(Args... args, Handler handler) const {
			m_multicast(args..., handler);
		}

	private:
		MulticastType m_multicast;

		bool Contains(const DelegateType& delegate) const {
			return m_multicast == delegate && m_multicast.size() == 1;
		}
	};

	template<typename OwnerType, typename Signature>
	class Event;

	template<typename OwnerType, typename R, typename... Args>
	class Event<OwnerType, R(Args...)> {
	public:
		using MulticastType = SA::multicast_delegate<R(Args...)>;
		using DelegateType = SA::delegate<R(Args...)>;

		explicit Event(OwnerType* owner) : m_owner(owner) {
			if (owner == nullptr) {
				throw std::invalid_argument("Event owner cannot be nullptr");
			}
		}

		Event(std::nullptr_t) = delete;

		template<R(*Function)(Args...)>
		void AddListener() {
			auto delegate = DelegateType::template create<Function>();

			if (Contains(delegate))
				return;

			m_multicast += delegate;
		}

		template<typename T, R(T::* Method)(Args...)>
		void AddListener(T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (Contains(delegate))
				return;

			m_multicast += delegate;
		}

		template<typename T, R(T::* Method)(Args...) const>
		void AddListener(const T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (Contains(delegate))
				return;

			m_multicast += delegate;
		}

		template<typename Lambda>
		void AddListenerLambda(const Lambda& lambda) {
			auto delegate = DelegateType::template create<Lambda>(lambda);
			m_multicast += delegate;
		}

		template<R(*Function)(Args...)>
		bool RemoveListener() {
			auto delegate = DelegateType::template create<Function>();

			if (!Contains(delegate))
				return false;

			m_multicast -= delegate;
			return true;
		}

		template<typename T, R(T::* Method)(Args...)>
		bool RemoveListener(T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (!Contains(delegate))
				return false;

			m_multicast -= delegate;
			return true;
		}

		template<typename T, R(T::* Method)(Args...) const>
		bool RemoveListener(const T* instance) {
			auto delegate = DelegateType::template create<T, Method>(instance);

			if (!Contains(delegate))
				return false;

			m_multicast -= delegate;
			return true;
		}

		void Clear() {
			m_multicast.clear();
		}

		size_t Count() const {
			return m_multicast.size();
		}

	private:
		OwnerType* m_owner;
		MulticastType m_multicast;

		friend OwnerType;

		bool Contains(const DelegateType& delegate) const {
			return m_multicast == delegate && m_multicast.size() == 1;
		}

		void Invoke(OwnerType* owner, Args... args) {
			if (owner != m_owner)
				throw std::runtime_error("Event invoked by non-owner");

			if constexpr (std::is_same_v<R, void>) {
				m_multicast(args...);
			}
			else {
				return m_multicast(args...);
			}
		}

		auto operator()(OwnerType* owner, Args... args) {
			return Invoke(owner, args...);
		}
	};

} // namespace MMMEngine