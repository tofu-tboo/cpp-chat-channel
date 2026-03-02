#ifndef __UNIV_KEY_H__
#define __UNIV_KEY_H__

#include <variant>
#include <string>
#include <type_traits>
#include <functional>

#include "class.h"

#define KEY(v)	(UnivKey(v))

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
				return std::hash<std::decay_t<decltype(arg)>>{}(arg);
			}, k.key) ^ (k.key.index() << 1);
		}
};

#endif
