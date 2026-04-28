#pragma once

#include "warp/codegen/detail/type_traits.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <utility>

namespace warp::codegen {

template <typename T>
struct json_object_contract;

enum class json_validation_mode {
	skip,
	enforce,
};

template <typename T>
struct json_field_validation {
	[[nodiscard]] constexpr bool empty() const noexcept {
		return true;
	}
};

template <>
struct json_field_validation<std::string> {
	std::optional<std::size_t> min_length {};
	std::optional<std::size_t> max_length {};

	[[nodiscard]] constexpr bool empty() const noexcept {
		return !min_length.has_value() && !max_length.has_value();
	}
};

template <>
struct json_field_validation<std::int64_t> {
	std::optional<std::int64_t> min {};
	std::optional<std::int64_t> max {};

	[[nodiscard]] constexpr bool empty() const noexcept {
		return !min.has_value() && !max.has_value();
	}
};

template <>
struct json_field_validation<double> {
	std::optional<double> min {};
	std::optional<double> max {};

	[[nodiscard]] constexpr bool empty() const noexcept {
		return !min.has_value() && !max.has_value();
	}
};

namespace detail {

template <typename Contract>
using contract_fields_type = std::remove_cvref_t<decltype(Contract::fields)>;

template <typename MemberPtr>
struct member_object_pointer_traits;

template <typename Class, typename Value>
struct member_object_pointer_traits<Value Class::*> {
	using class_type = Class;
	using value_type = Value;
};

template <typename MemberPtr>
using member_validation_type = json_field_validation<
    typename optional_value_traits<typename member_object_pointer_traits<MemberPtr>::value_type>::value_type>;

template <typename Validation>
concept string_length_validation = requires(const Validation &validation) {
	validation.min_length;
	validation.max_length;
};

template <typename Validation>
concept numeric_range_validation = requires(const Validation &validation) {
	validation.min;
	validation.max;
};

template <typename Value, typename Validation>
void validate_json_field_value(const Value &value, const Validation &validation, std::string_view wire_name,
                               std::string_view type_name) {
	if constexpr (string_length_validation<Validation>) {
		if (validation.min_length.has_value() && value.size() < *validation.min_length) {
			throw std::invalid_argument("field '" + std::string(wire_name) + "' for " + std::string(type_name) +
			                            " must have length >= " + std::to_string(*validation.min_length));
		}
		if (validation.max_length.has_value() && value.size() > *validation.max_length) {
			throw std::invalid_argument("field '" + std::string(wire_name) + "' for " + std::string(type_name) +
			                            " must have length <= " + std::to_string(*validation.max_length));
		}
	}

	if constexpr (numeric_range_validation<Validation>) {
		if (validation.min.has_value() && value < *validation.min) {
			throw std::invalid_argument("field '" + std::string(wire_name) + "' for " + std::string(type_name) +
			                            " must be >= " + std::to_string(*validation.min));
		}
		if (validation.max.has_value() && value > *validation.max) {
			throw std::invalid_argument("field '" + std::string(wire_name) + "' for " + std::string(type_name) +
			                            " must be <= " + std::to_string(*validation.max));
		}
	}
}

template <typename Model, typename Descriptor>
void parse_json_field(Model &out, const boost::json::object &obj, const Descriptor &descriptor,
                      std::string_view type_name, json_validation_mode validation_mode) {
	const auto *raw = obj.if_contains(descriptor.wire_name);
	if (raw == nullptr) {
		if constexpr (Descriptor::required) {
			throw std::invalid_argument("missing required field '" + std::string(descriptor.wire_name) + "' for " +
			                            std::string(type_name));
		}
		return;
	}

	try {
		using storage_type = typename Descriptor::storage_type;
		using storage_traits = optional_value_traits<storage_type>;
		using value_type = typename Descriptor::value_type;

		auto parsed = boost::json::value_to<value_type>(*raw);
		if (validation_mode == json_validation_mode::enforce) {
			validate_json_field_value(parsed, descriptor.validation, descriptor.wire_name, type_name);
		}

		if constexpr (storage_traits::is_optional) {
			out.*(descriptor.member) = storage_type(std::move(parsed));
		} else {
			out.*(descriptor.member) = std::move(parsed);
		}
	} catch (const std::exception &ex) {
		throw std::invalid_argument("invalid field '" + std::string(descriptor.wire_name) + "' for " +
		                            std::string(type_name) + ": " + ex.what());
	}
}

template <typename Contract, typename Model, std::size_t... Indices>
void parse_json_fields(Model &out, const boost::json::object &obj, json_validation_mode validation_mode,
                       std::index_sequence<Indices...>) {
	(parse_json_field(out, obj, std::get<Indices>(Contract::fields), Contract::type_name, validation_mode), ...);
}

template <typename Model, typename Descriptor>
void serialize_json_field(boost::json::object &obj, Model &&input, const Descriptor &descriptor) {
	using storage_type = typename Descriptor::storage_type;
	using storage_traits = optional_value_traits<storage_type>;

	if constexpr (std::is_rvalue_reference_v<Model &&>) {
		auto &&field_value = (std::move(input)).*(descriptor.member);
		if constexpr (storage_traits::is_optional) {
			if (field_value.has_value()) {
				obj[descriptor.wire_name] = boost::json::value_from(std::move(*field_value));
			}
		} else {
			obj[descriptor.wire_name] = boost::json::value_from(std::move(field_value));
		}
	} else {
		const auto &field_value = input.*(descriptor.member);
		if constexpr (storage_traits::is_optional) {
			if (field_value.has_value()) {
				obj[descriptor.wire_name] = boost::json::value_from(*field_value);
			}
		} else {
			obj[descriptor.wire_name] = boost::json::value_from(field_value);
		}
	}
}

template <typename Contract, typename Model, std::size_t... Indices>
void serialize_json_fields(boost::json::object &obj, Model &&input, std::index_sequence<Indices...>) {
	(serialize_json_field(obj, std::forward<Model>(input), std::get<Indices>(Contract::fields)), ...);
}

} // namespace detail

template <bool Required, typename MemberPtr, typename Validation = detail::member_validation_type<MemberPtr>>
struct json_field_descriptor {
	using member_traits = detail::member_object_pointer_traits<MemberPtr>;
	using class_type = typename member_traits::class_type;
	using storage_type = typename member_traits::value_type;
	using value_type = typename detail::optional_value_traits<storage_type>::value_type;
	using validation_type = Validation;

	static constexpr bool required = Required;

	constexpr json_field_descriptor(const char *wire_name_in, MemberPtr member_in, Validation validation_in = {})
	    : wire_name(wire_name_in), member(member_in), validation(std::move(validation_in)) {
		static_assert(std::is_member_object_pointer_v<MemberPtr>);
		static_assert(std::is_same_v<Validation, detail::member_validation_type<MemberPtr>>);
		static_assert(required == !detail::optional_value_traits<storage_type>::is_optional);
	}

	const char *wire_name;
	MemberPtr member;
	Validation validation {};
};

template <typename MemberPtr>
constexpr auto make_required_json_field(const char *wire_name, MemberPtr member,
                                        detail::member_validation_type<MemberPtr> validation = {}) {
	return json_field_descriptor<true, MemberPtr>(wire_name, member, std::move(validation));
}

template <typename MemberPtr>
constexpr auto make_optional_json_field(const char *wire_name, MemberPtr member,
                                        detail::member_validation_type<MemberPtr> validation = {}) {
	return json_field_descriptor<false, MemberPtr>(wire_name, member, std::move(validation));
}

template <typename T>
concept json_contract_type = requires {
	json_object_contract<std::remove_cvref_t<T>>::type_name;
	json_object_contract<std::remove_cvref_t<T>>::fields;
};

template <typename T>
    requires json_contract_type<T>
T parse_json_object(const boost::json::value &value,
                    json_validation_mode validation_mode = json_validation_mode::skip) {
	using contract = json_object_contract<T>;
	const auto *obj = value.if_object();
	if (obj == nullptr) {
		throw std::invalid_argument("expected JSON object for " + std::string(contract::type_name));
	}

	T out;
	detail::parse_json_fields<contract>(
	    out, *obj, validation_mode,
	    std::make_index_sequence<std::tuple_size_v<detail::contract_fields_type<contract>>> {});
	return out;
}

template <typename T>
    requires json_contract_type<T>
void serialize_json_object(boost::json::value &value, T &&input) {
	using contract = json_object_contract<std::remove_cvref_t<T>>;
	boost::json::object obj;
	obj.reserve(std::tuple_size_v<detail::contract_fields_type<contract>>);
	detail::serialize_json_fields<contract>(
	    obj, std::forward<T>(input),
	    std::make_index_sequence<std::tuple_size_v<detail::contract_fields_type<contract>>> {});
	value = std::move(obj);
}

} // namespace warp::codegen
