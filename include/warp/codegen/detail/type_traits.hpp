#pragma once

#include <optional>
#include <type_traits>

namespace warp::codegen::detail {

template <typename T>
inline constexpr bool always_false_v = false;

template <typename... Ts>
struct type_list {};

template <typename MemberFn>
struct member_function_traits;

#define WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(QUALIFIERS, NOEXCEPT_SPEC)                                                 \
	template <typename Class, typename Return, typename Arg>                                                           \
	struct member_function_traits<Return (Class::*)(Arg) QUALIFIERS NOEXCEPT_SPEC> {                                   \
		using class_type = Class;                                                                                      \
		using return_type = Return;                                                                                    \
		using argument_type = std::remove_cvref_t<Arg>;                                                                \
	};                                                                                                                 \
	template <typename Class, typename Return>                                                                         \
	struct member_function_traits<Return (Class::*)() QUALIFIERS NOEXCEPT_SPEC> {                                      \
		using class_type = Class;                                                                                      \
		using return_type = Return;                                                                                    \
		using value_type = std::remove_cvref_t<Return>;                                                                \
	};

WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(, )
WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(const, )
WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(&, )
WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(const &, )
WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(&&, )
WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(const &&, )
WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(, noexcept)
WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(const, noexcept)
WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(&, noexcept)
WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(const &, noexcept)
WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(&&, noexcept)
WARP_CODEGEN_MEMBER_FUNCTION_TRAITS(const &&, noexcept)

#undef WARP_CODEGEN_MEMBER_FUNCTION_TRAITS

template <typename T>
struct optional_value_traits {
	static constexpr bool is_optional = false;
	using value_type = T;
};

template <typename T>
struct optional_value_traits<std::optional<T>> {
	static constexpr bool is_optional = true;
	using value_type = T;
};

} // namespace warp::codegen::detail
