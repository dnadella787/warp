#pragma once

#include <optional>
#include <type_traits>

namespace warp::codegen::detail {

template <typename... Ts>
struct type_list {};

/**
 * declared but not defined so that the only possible specialization
 * that the compiler will allow is one that is below. We can't use the
 * specialization below outright because it is already specialized,
 * using function, type specify in the template parameter and a specialization
 * without an initial definition is not allowed. This provides an additional
 * benefit of only allowing the below specialization.
 */
template <typename MemberFn>
struct member_function_traits;

/**
 * take a reference to a member function with a single argument and infer the class
 * type, the return type, and type of the single argument that the function takes in
 * with the reference type removed. Also filter by the exact qualifiers like no
 * except const ref, etc because each qualifier for the function actually ends up being
 * a different type entirely.
 *
 * Example:

  struct Request {
      void set_id(std::string);
  };

  For decltype(&Request::set_id), the trait gives:

  - class_type = Request
  - return_type = void
  - argument_type = std::string
 *
 * do the same thing for a function that takes in no arguments as well.
 *
 * Example:

  struct Request {
      std::string id() const;
  };

  the trait gives:

  - class_type = Request
  - return_type = std::string
  - value_type = std::string
 */
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
