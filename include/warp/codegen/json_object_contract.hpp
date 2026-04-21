#pragma once

#include "warp/codegen/detail/type_traits.hpp"

#include <boost/json/object.hpp>
#include <boost/json/value.hpp>
#include <boost/json/value_from.hpp>
#include <boost/json/value_to.hpp>

#include <functional>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>

namespace warp::codegen {

template <typename T>
struct json_object_contract;

namespace detail {

template <typename Contract>
using contract_fields_type = std::remove_cvref_t<decltype(Contract::fields)>;

template <typename Model, typename Descriptor>
void parse_json_field(Model &out, const boost::json::object &obj, const Descriptor &descriptor,
                      std::string_view type_name) {
	const auto *raw = obj.if_contains(descriptor.wire_name);
	if (raw == nullptr) {
		if constexpr (Descriptor::required) {
			throw std::invalid_argument("missing required field '" + std::string(descriptor.wire_name) + "' for " +
			                            std::string(type_name));
		}
		return;
	}

	try {
		using setter_argument_type = typename Descriptor::setter_argument_type;
		using setter_value_traits = optional_value_traits<setter_argument_type>;
		using value_type = typename setter_value_traits::value_type;

		auto parsed = boost::json::value_to<value_type>(*raw);
		if constexpr (setter_value_traits::is_optional) {
			std::invoke(descriptor.setter, out, setter_argument_type(std::move(parsed)));
		} else {
			std::invoke(descriptor.setter, out, std::move(parsed));
		}
	} catch (const std::exception &ex) {
		throw std::invalid_argument("invalid field '" + std::string(descriptor.wire_name) + "' for " +
		                            std::string(type_name) + ": " + ex.what());
	}
}

template <typename Contract, typename Model, std::size_t... Indices>
void parse_json_fields(Model &out, const boost::json::object &obj, std::index_sequence<Indices...>) {
	(parse_json_field(out, obj, std::get<Indices>(Contract::fields), Contract::type_name), ...);
}

template <typename Model, typename Descriptor>
void serialize_json_field(boost::json::object &obj, Model &&input, const Descriptor &descriptor) {
	using storage_type = typename Descriptor::storage_type;
	using storage_traits = optional_value_traits<storage_type>;

	if constexpr (std::is_rvalue_reference_v<Model &&>) {
		if constexpr (storage_traits::is_optional) {
			auto field_value = std::invoke(descriptor.move_getter, std::move(input));
			if (field_value.has_value()) {
				obj[descriptor.wire_name] = boost::json::value_from(std::move(*field_value));
			}
		} else {
			obj[descriptor.wire_name] = boost::json::value_from(std::invoke(descriptor.move_getter, std::move(input)));
		}
	} else {
		const auto &stable_input = input;
		if constexpr (storage_traits::is_optional) {
			const auto &field_value = std::invoke(descriptor.const_getter, stable_input);
			if (field_value.has_value()) {
				obj[descriptor.wire_name] = boost::json::value_from(*field_value);
			}
		} else {
			obj[descriptor.wire_name] = boost::json::value_from(std::invoke(descriptor.const_getter, stable_input));
		}
	}
}

template <typename Contract, typename Model, std::size_t... Indices>
void serialize_json_fields(boost::json::object &obj, Model &&input, std::index_sequence<Indices...>) {
	(serialize_json_field(obj, std::forward<Model>(input), std::get<Indices>(Contract::fields)), ...);
}

} // namespace detail

template <bool Required, typename Setter, typename ConstGetter, typename MoveGetter>
struct json_field_descriptor {
	using setter_traits = detail::member_function_traits<Setter>;
	using const_getter_traits = detail::member_function_traits<ConstGetter>;
	using move_getter_traits = detail::member_function_traits<MoveGetter>;
	using setter_argument_type = typename setter_traits::argument_type;
	using storage_type = typename const_getter_traits::value_type;

	static constexpr bool required = Required;

	constexpr json_field_descriptor(const char *wire_name_in, Setter setter_in, ConstGetter const_getter_in,
	                                MoveGetter move_getter_in)
	    : wire_name(wire_name_in), setter(setter_in), const_getter(const_getter_in), move_getter(move_getter_in) {
		static_assert(std::is_same_v<typename setter_traits::class_type, typename const_getter_traits::class_type>);
		static_assert(std::is_same_v<typename setter_traits::class_type, typename move_getter_traits::class_type>);
		static_assert(std::is_same_v<storage_type, typename move_getter_traits::value_type>);
		static_assert(std::is_same_v<setter_argument_type, storage_type>);
		static_assert(required == !detail::optional_value_traits<storage_type>::is_optional);
	}

	const char *wire_name;
	Setter setter;
	ConstGetter const_getter;
	MoveGetter move_getter;
};

template <typename Setter, typename ConstGetter, typename MoveGetter>
constexpr auto make_required_json_field(const char *wire_name, Setter setter, ConstGetter const_getter,
                                        MoveGetter move_getter) {
	return json_field_descriptor<true, Setter, ConstGetter, MoveGetter>(wire_name, setter, const_getter, move_getter);
}

template <typename Setter, typename ConstGetter, typename MoveGetter>
constexpr auto make_optional_json_field(const char *wire_name, Setter setter, ConstGetter const_getter,
                                        MoveGetter move_getter) {
	return json_field_descriptor<false, Setter, ConstGetter, MoveGetter>(wire_name, setter, const_getter, move_getter);
}

template <typename T>
concept json_contract_type = requires {
	json_object_contract<std::remove_cvref_t<T>>::type_name;
	json_object_contract<std::remove_cvref_t<T>>::fields;
};

template <typename T>
    requires json_contract_type<T>
T parse_json_object(const boost::json::value &value) {
	using contract = json_object_contract<T>;
	const auto *obj = value.if_object();
	if (obj == nullptr) {
		throw std::invalid_argument("expected JSON object for " + std::string(contract::type_name));
	}

	T out;
	detail::parse_json_fields<contract>(
	    out, *obj, std::make_index_sequence<std::tuple_size_v<detail::contract_fields_type<contract>>> {});
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
