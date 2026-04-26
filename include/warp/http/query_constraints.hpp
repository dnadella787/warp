#pragma once

#include <array>
#include <concepts>
#include <cstddef>
#include <string_view>

#include "route_fixed_string.hpp"

namespace warp::http {

enum class query_constraint_presence {
	required,
	optional,
	forbidden,
};

struct query_constraint_descriptor {
	std::string_view name;
	query_constraint_presence presence {query_constraint_presence::required};
	bool has_exact_value {};
	std::string_view exact_value;
};

template <typename T>
concept query_constraint = requires {
	{ T::descriptor() } -> std::same_as<query_constraint_descriptor>;
};

namespace detail {

enum class query_constraint_name_error {
	none,
	empty_name,
	contains_separator,
	reserved_priority_name,
};

/** compile time query param name validation
 * 1. cannot be empty
 * 2. name cannot be a reserved priority keyword
 * 3. param cannot contain significant chars & = ? # !  (_ - . allowed)
 */
[[nodiscard]] constexpr query_constraint_name_error validate_query_constraint_name(std::string_view name) noexcept {
	if (name.empty())
		return query_constraint_name_error::empty_name;
	if (name == "priority" || name == "_priority" || name == "__priority" || name == "__warp_priority")
		return query_constraint_name_error::reserved_priority_name;
	if (name.find_first_of("&=?#!~") != std::string_view::npos)
		return query_constraint_name_error::contains_separator;
	return query_constraint_name_error::none;
}

/**
 * we use always_false_v here because Error is a value of type query_constraint_name_error, not a type itself
 * we call this function as fail_query_constraint_name_validation<query_constraint_name_error::empty_name>() not
 * fail_query_constraint_name_validation<query_constraint_name_error>()
 */
template <query_constraint_name_error Error>
consteval void fail_query_constraint_name_validation() {
	static_assert(Error != query_constraint_name_error::empty_name, "query constraint name must not be empty");
	static_assert(Error != query_constraint_name_error::contains_separator,
	              "query constraint name must not contain '&', '=', '?', '#', '!', or '~'");
	static_assert(Error != query_constraint_name_error::reserved_priority_name,
	              "query constraint name is reserved for route priority metadata");
}

template <fixed_string Name>
consteval void checked_query_constraint_name() {
	constexpr auto validation = validate_query_constraint_name(Name.view());
	if constexpr (validation != query_constraint_name_error::none)
		fail_query_constraint_name_validation<validation>();
}

template <query_constraint... QueryConstraints>
[[nodiscard]] consteval bool has_duplicate_query_constraints() {
	constexpr std::array<query_constraint_descriptor, sizeof...(QueryConstraints)> descriptors {
	    QueryConstraints::descriptor()...};
	for (std::size_t i = 0; i < descriptors.size(); ++i) {
		for (std::size_t j = i + 1; j < descriptors.size(); ++j) {
			if (descriptors[i].name == descriptors[j].name)
				return true;
		}
	}
	return false;
}

} // namespace detail

template <fixed_string Name, query_constraint_presence Presence, fixed_string Value = "">
struct basic_query_constraint {
private:
	/**
	 * compile time check to validate the name. `consteval` forces this evaluation
	 * to happen at compile time.
	 */
	static constexpr bool validated_ = []() consteval {
		detail::checked_query_constraint_name<Name>();
		return true;
	}();

public:
	static constexpr auto literal = Name;
	static constexpr auto presence = Presence;
	static constexpr bool has_exact_value = Value.view().size() > 0;

	[[nodiscard]] static constexpr std::string_view name_view() noexcept {
		static_cast<void>(validated_);
		return Name.view();
	}

	[[nodiscard]] static constexpr std::string_view value_view() noexcept {
		return Value.view();
	}

	[[nodiscard]] static constexpr query_constraint_descriptor descriptor() noexcept {
		static_cast<void>(validated_);
		return {
		    .name = Name.view(), .presence = Presence, .has_exact_value = has_exact_value, .exact_value = Value.view()};
	}
};

template <fixed_string Name>
using required_query = basic_query_constraint<Name, query_constraint_presence::required>;

template <fixed_string Name>
using optional_query = basic_query_constraint<Name, query_constraint_presence::optional>;

template <fixed_string Name>
using forbidden_query = basic_query_constraint<Name, query_constraint_presence::forbidden>;

template <fixed_string Name, fixed_string Value>
using required_query_value = basic_query_constraint<Name, query_constraint_presence::required, Value>;

template <fixed_string Name, fixed_string Value>
using optional_query_value = basic_query_constraint<Name, query_constraint_presence::optional, Value>;

} // namespace warp::http
