#ifndef __SERVICE_WORKER_H__
#define __SERVICE_WORKER_H__

#define KEY(v)	(UnivKey(v))

#include <deque>
#include <functional>
#include <unordered_map>
#include <list>
#include <string>
#include <variant>
#include <type_traits>
#include <optional>
#include <stdexcept>
#include <shared_mutex>
#include <mutex>
#include <atomic>

#include "class.h"

class UnivKey {
	var_public:
		std::variant<std::string, long long, double> key;
	func_public:
		// String constructors
		UnivKey(const char* s) : key(std::string(s)) {}
		UnivKey(const std::string& s) : key(s) {}

		// Template constructor for integral types (e.g., int, short, long)
		template<typename T, std::enable_if_t<std::is_integral_v<T>, bool> = true>
		UnivKey(T value) : key(static_cast<long long>(value)) {}

		// Template constructor for floating point types (e.g., float, double)
		template<typename T, std::enable_if_t<std::is_floating_point_v<T>, bool> = true>
		UnivKey(T value) : key(static_cast<double>(value)) {}

		bool operator==(const UnivKey& other) const { return key == other.key; }
};

class UnivKeyHash {
	func_public:
		std::size_t operator()(const UnivKey& k) const {
			return std::visit([](auto&& arg) {
				return std::hash<std::decay_t<decltype(arg)>>{}(arg); // decltype이 참조형일 수도 있으므로 decay_t로 참조형을 제거함.
			}, k.key) ^ (k.key.index() << 1);
		}
};

template <typename T>
class KeyList {
	type_public:
		struct Node {
			std::optional<UnivKey> name;
			T value;
		};

		using iterator = typename std::list<Node>::iterator;
		using const_iterator = typename std::list<Node>::const_iterator;
	var_private:
		std::list<Node> list;
		std::unordered_map<UnivKey, typename std::list<Node>::iterator, UnivKeyHash> map;
		mutable std::shared_mutex mtx;
	func_public:
		std::shared_mutex& get_mutex() const { return mtx; }

		void push_back(const UnivKey& name, const T& value) {
			std::unique_lock lock(mtx);
			if (map.find(name) != map.end()) throw std::runtime_error("Key already exists.");
			list.push_back({name, value});
			map[name] = std::prev(list.end());
		}

		void push_back(const UnivKey& name, T&& value) {
			std::unique_lock lock(mtx);
			if (map.find(name) != map.end()) throw std::runtime_error("Key already exists.");
			list.push_back({name, std::move(value)});
			map[name] = std::prev(list.end());
		}
		
		void push_back(const T& value) {
			std::unique_lock lock(mtx);
			list.push_back({std::nullopt, value});
		}

		void push_back(T&& value) {
			std::unique_lock lock(mtx);
			list.push_back({std::nullopt, std::move(value)});
		}

		void push_front(const UnivKey& name, const T& value) {
			std::unique_lock lock(mtx);
			if (map.find(name) != map.end()) throw std::runtime_error("Key already exists.");
			list.push_front({name, value});
			map[name] = list.begin();
		}

		void push_front(const UnivKey& name, T&& value) {
			std::unique_lock lock(mtx);
			if (map.find(name) != map.end()) throw std::runtime_error("Key already exists.");
			list.push_front({name, std::move(value)});
			map[name] = list.begin();
		}

		void push_front(const T& value) {
			std::unique_lock lock(mtx);
			list.push_front({std::nullopt, value});
		}

		void push_front(T&& value) {
			std::unique_lock lock(mtx);
			list.push_front({std::nullopt, std::move(value)});
		}

		void pop_back() {
			std::unique_lock lock(mtx);
			if (list.empty()) return;
			if (list.back().name.has_value()) {
				map.erase(list.back().name.value());
			}
			list.pop_back();
		}

		void pop_front() {
			std::unique_lock lock(mtx);
			if (list.empty()) return;
			if (list.front().name.has_value()) {
				map.erase(list.front().name.value());
			}
			list.pop_front();
		}

		void remove(const UnivKey& name) {
			std::unique_lock lock(mtx);
			auto it = map.find(name);
			if (it != map.end()) {
				list.erase(it->second);
				map.erase(it);
			}
		}

		bool empty() const { std::shared_lock lock(mtx); return list.empty(); }
		size_t size() const { std::shared_lock lock(mtx); return list.size(); }
		
		T& front() { std::shared_lock lock(mtx); return list.front().value; }
		T& back() { std::shared_lock lock(mtx); return list.back().value; }

		iterator begin() { return list.begin(); }
		iterator end() { return list.end(); }
		const_iterator begin() const { return list.begin(); }
		const_iterator end() const { return list.end(); }
		const_iterator cbegin() const { return list.cbegin(); }
		const_iterator cend() const { return list.cend(); }
};

class ServiceWorker {
	type_public:
		enum Type { PRE = 0, POST, BG };
	type_private:
		// struct Task {
		// 	std::function<void()> task;
		// 	bool done;
		// };
		using Task = std::function<void()>;
	var_private:
		KeyList<Task> pre_poll;
		KeyList<Task> post_poll;
		KeyList<Task> bg_proc;
	func_public:
		ServiceWorker();
		~ServiceWorker();

		void add(Type type, const UnivKey& key, Task task);
		void add(Type type, Task task);
		void remove(Type type, const UnivKey& key);
		void run(Type type);
		void clear(Type type);
};

#endif