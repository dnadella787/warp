#include "codegen/data_class_emitter.hpp"
#include "codegen/model.hpp"
#include "codegen/spec_parser.hpp"

#include <gtest/gtest.h>

#include <string>

namespace {

using warp::codegen::build_api_model;
using warp::codegen::data_class_emitter;
using warp::codegen::parse_api_spec;

std::size_t substring_count(const std::string &text, const std::string &needle) {
	if (needle.empty()) {
		return 0;
	}

	std::size_t count = 0;
	std::size_t pos = 0;
	while ((pos = text.find(needle, pos)) != std::string::npos) {
		++count;
		pos += needle.size();
	}
	return count;
}

TEST(DataClassEmitterTest, EmitsBodySchemasAndEndpointContracts) {
	const auto spec = parse_api_spec(R"(
cpp_namespace: generated_api
resources:
  - name: users
    endpoints:
      - name: create_user
        method: POST
        path: /users/{user_id}
        request:
          parameters:
            - name: user_id
              in: path
              type: string
            - name: verbose
              in: query
              type: bool
              required: false
          body:
            type: object
            fields:
              - name: addresses
                type: array
                items:
                  type: object
                  fields:
                    - name: city
                      type: string
              - name: profile
                type: object
                fields:
                  - name: display-name
                    type: string
        response:
          status: 201
          body:
            type: object
            fields:
              - name: id
                type: int64
)");

	const auto output = data_class_emitter().emit_header(build_api_model(spec));
	EXPECT_NE(output.find("namespace generated_api {"), std::string::npos);
	EXPECT_NE(output.find("#include \"warp/codegen/json_object_contract.hpp\""), std::string::npos);
	EXPECT_NE(output.find("class users_create_user_request_body_addresses_item {"), std::string::npos);
	EXPECT_NE(output.find("class users_create_user_request_body_profile {"), std::string::npos);
	EXPECT_NE(output.find("class users_create_user_request_body {"), std::string::npos);
	EXPECT_NE(output.find("class users_create_user_request {"), std::string::npos);
	EXPECT_NE(output.find("std::string user_id_ {};"), std::string::npos);
	EXPECT_NE(output.find("std::optional<bool> verbose_ {};"), std::string::npos);
	EXPECT_NE(output.find("users_create_user_request_body body_ {};"), std::string::npos);
	EXPECT_NE(output.find("class users_create_user_response_body {"), std::string::npos);
	EXPECT_NE(output.find("class users_create_user_response {"), std::string::npos);
	EXPECT_NE(output.find("static constexpr unsigned status_code = 201;"), std::string::npos);
	EXPECT_NE(output.find("[[nodiscard]] static Builder builder() {"), std::string::npos);
	EXPECT_NE(output.find("users_create_user_request &set_user_id(std::string value) {"), std::string::npos);
	EXPECT_NE(output.find("[[nodiscard]] const std::string &user_id() const & noexcept {"), std::string::npos);
	EXPECT_NE(output.find("requires warp::codegen::json_contract_type<T>"), std::string::npos);
	EXPECT_NE(output.find("struct json_object_contract<generated_api::users_create_user_request_body_profile> {"),
	          std::string::npos);
	EXPECT_NE(output.find("make_required_json_field(\"display-name\","), std::string::npos);
	EXPECT_EQ(output.find("inline users_create_user_request_body tag_invoke("), std::string::npos);
	EXPECT_EQ(output.find("users_create_user_request_body &&input) {"), std::string::npos);
	EXPECT_EQ(substring_count(output, "class users_create_user_request_body_profile"), 1U);
}

TEST(DataClassEmitterTest, OmitsBodyMembersWhenRequestOrResponseHasNoBody) {
	const auto spec = parse_api_spec(R"(
resources:
  - name: system
    endpoints:
      - name: health
        path: /health
        response:
          status: 204
)");

	const auto output = data_class_emitter().emit_header(build_api_model(spec));

	EXPECT_NE(output.find("class system_health_request {"), std::string::npos);
	EXPECT_NE(output.find("class system_health_response {"), std::string::npos);
	EXPECT_NE(output.find("static constexpr unsigned status_code = 204;"), std::string::npos);
	EXPECT_EQ(substring_count(output, " body_ {};"), 0U);
}

} // namespace
