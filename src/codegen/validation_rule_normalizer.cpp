#include "codegen/validation_rule_normalizer.hpp"

#include <string>
#include <variant>

#include "codegen/model_diagnostics.hpp"

namespace warp::codegen::detail {

namespace {

[[nodiscard]] bool is_validation_value_integral(const numeric_validation_value &value) {
	return std::holds_alternative<std::int64_t>(value);
}

[[nodiscard]] std::int64_t as_int64_validation_value(const numeric_validation_value &value) {
	return std::get<std::int64_t>(value);
}

[[nodiscard]] double as_double_validation_value(const numeric_validation_value &value) {
	if (const auto *integer = std::get_if<std::int64_t>(&value)) {
		return static_cast<double>(*integer);
	}
	return std::get<double>(value);
}

} // namespace

validation_rules validation_rule_normalizer::normalize(const validation_rule_spec &input, value_kind kind,
                                                       source_span fallback_span, std::string_view subject) const {
	validation_rules output;

	auto reject = [&](source_span span, std::string message) {
		fail(validation_span_or(span, fallback_span), "model.invalid_validation_rule", std::move(message));
	};

	switch (kind) {
	case value_kind::string_value:
		if (input.min.has_value()) {
			reject(input.min_span, std::string(subject) + " cannot use numeric rule 'min' with string type");
		}
		if (input.max.has_value()) {
			reject(input.max_span, std::string(subject) + " cannot use numeric rule 'max' with string type");
		}
		output.min_length = input.min_length;
		output.max_length = input.max_length;
		if (output.min_length.has_value() && output.max_length.has_value() && *output.min_length > *output.max_length) {
			fail(validation_span_or(input.max_length_span, fallback_span), "model.invalid_validation_range",
			     std::string(subject) + " has min_length greater than max_length");
		}
		return output;
	case value_kind::int64_value:
		if (input.min_length.has_value()) {
			reject(input.min_length_span,
			       std::string(subject) + " cannot use string rule 'min_length' with int64 type");
		}
		if (input.max_length.has_value()) {
			reject(input.max_length_span,
			       std::string(subject) + " cannot use string rule 'max_length' with int64 type");
		}
		if (input.min.has_value()) {
			if (!is_validation_value_integral(*input.min)) {
				reject(input.min_span, std::string(subject) + " must use an integer value for rule 'min'");
			}
			output.min = as_int64_validation_value(*input.min);
		}
		if (input.max.has_value()) {
			if (!is_validation_value_integral(*input.max)) {
				reject(input.max_span, std::string(subject) + " must use an integer value for rule 'max'");
			}
			output.max = as_int64_validation_value(*input.max);
		}
		if (output.min.has_value() && output.max.has_value() &&
		    as_int64_validation_value(*output.min) > as_int64_validation_value(*output.max)) {
			fail(validation_span_or(input.max_span, fallback_span), "model.invalid_validation_range",
			     std::string(subject) + " has min greater than max");
		}
		return output;
	case value_kind::double_value:
		if (input.min_length.has_value()) {
			reject(input.min_length_span,
			       std::string(subject) + " cannot use string rule 'min_length' with double type");
		}
		if (input.max_length.has_value()) {
			reject(input.max_length_span,
			       std::string(subject) + " cannot use string rule 'max_length' with double type");
		}
		if (input.min.has_value()) {
			output.min = as_double_validation_value(*input.min);
		}
		if (input.max.has_value()) {
			output.max = as_double_validation_value(*input.max);
		}
		if (output.min.has_value() && output.max.has_value() &&
		    as_double_validation_value(*output.min) > as_double_validation_value(*output.max)) {
			fail(validation_span_or(input.max_span, fallback_span), "model.invalid_validation_range",
			     std::string(subject) + " has min greater than max");
		}
		return output;
	case value_kind::bool_value:
		break;
	case value_kind::object_value:
		break;
	case value_kind::array_value:
		break;
	}

	if (input.min.has_value()) {
		reject(input.min_span,
		       std::string(subject) + " cannot use validation rule 'min' with type " + std::string(to_string(kind)));
	}
	if (input.max.has_value()) {
		reject(input.max_span,
		       std::string(subject) + " cannot use validation rule 'max' with type " + std::string(to_string(kind)));
	}
	if (input.min_length.has_value()) {
		reject(input.min_length_span, std::string(subject) + " cannot use validation rule 'min_length' with type " +
		                                  std::string(to_string(kind)));
	}
	if (input.max_length.has_value()) {
		reject(input.max_length_span, std::string(subject) + " cannot use validation rule 'max_length' with type " +
		                                  std::string(to_string(kind)));
	}
	return output;
}

} // namespace warp::codegen::detail
