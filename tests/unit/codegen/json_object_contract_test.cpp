#include "warp/codegen/json_object_contract.hpp"

#include <gtest/gtest.h>

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <cstdint>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace json_contract_test_model {

struct address {
	std::string city {};
};

struct profile {
	std::string display_name {};
};

struct user {
	std::string name {};
	std::optional<std::string> nickname {};
	profile profile_data {};
	std::vector<address> addresses {};
};

struct validated_request {
	std::string name {};
	std::optional<std::string> nickname {};
	std::int64_t age {};
};

template <typename T>
    requires warp::codegen::json_contract_type<T>
inline T tag_invoke(boost::json::value_to_tag<T>, const boost::json::value &value) {
	return warp::codegen::parse_json_object<T>(value);
}

template <typename T>
    requires warp::codegen::json_contract_type<T>
inline void tag_invoke(boost::json::value_from_tag, boost::json::value &value, T &&input) {
	warp::codegen::serialize_json_object(value, std::forward<T>(input));
}

} // namespace json_contract_test_model

namespace warp::codegen {

template <>
struct json_object_contract<json_contract_test_model::address> {
	static constexpr std::string_view type_name = "address";
	static constexpr auto fields =
	    std::make_tuple(make_required_json_field("city", &json_contract_test_model::address::city));
};

template <>
struct json_object_contract<json_contract_test_model::profile> {
	static constexpr std::string_view type_name = "profile";
	static constexpr auto fields =
	    std::make_tuple(make_required_json_field("display_name", &json_contract_test_model::profile::display_name));
};

template <>
struct json_object_contract<json_contract_test_model::user> {
	static constexpr std::string_view type_name = "user";
	static constexpr auto fields =
	    std::make_tuple(make_required_json_field("name", &json_contract_test_model::user::name),
	                    make_optional_json_field("nickname", &json_contract_test_model::user::nickname),
	                    make_required_json_field("profile", &json_contract_test_model::user::profile_data),
	                    make_required_json_field("addresses", &json_contract_test_model::user::addresses));
};

template <>
struct json_object_contract<json_contract_test_model::validated_request> {
	static constexpr std::string_view type_name = "validated_request";
	static constexpr auto fields = std::make_tuple(
	    make_required_json_field("name", &json_contract_test_model::validated_request::name,
	                             json_field_validation<std::string> {.min_length = 3U, .max_length = 8U}),
	    make_optional_json_field("nickname", &json_contract_test_model::validated_request::nickname,
	                             json_field_validation<std::string> {.max_length = 5U}),
	    make_required_json_field("age", &json_contract_test_model::validated_request::age,
	                             json_field_validation<std::int64_t> {.min = 18, .max = 65}));
};

} // namespace warp::codegen

namespace {

using json_contract_test_model::user;
using json_contract_test_model::validated_request;

TEST(JsonObjectContractTest, ParsesAndSerializesNestedPublicMemberObjects) {
	const auto parsed = boost::json::parse(
	    R"({"name":"Alice","nickname":"ally","profile":{"display_name":"Alice A."},"addresses":[{"city":"Austin"},{"city":"Dallas"}]})");
	const auto typed = boost::json::value_to<user>(parsed);

	EXPECT_EQ(typed.name, "Alice");
	ASSERT_TRUE(typed.nickname.has_value());
	EXPECT_EQ(*typed.nickname, "ally");
	EXPECT_EQ(typed.profile_data.display_name, "Alice A.");
	ASSERT_EQ(typed.addresses.size(), 2U);
	EXPECT_EQ(typed.addresses[0].city, "Austin");
	EXPECT_EQ(typed.addresses[1].city, "Dallas");

	const auto serialized = boost::json::value_from(user(typed));
	const auto &obj = serialized.as_object();
	EXPECT_EQ(std::string(obj.at("name").as_string()), "Alice");
	EXPECT_EQ(std::string(obj.at("nickname").as_string()), "ally");
	EXPECT_EQ(std::string(obj.at("profile").as_object().at("display_name").as_string()), "Alice A.");
	ASSERT_EQ(obj.at("addresses").as_array().size(), 2U);
	EXPECT_EQ(std::string(obj.at("addresses").as_array()[0].as_object().at("city").as_string()), "Austin");
}

TEST(JsonObjectContractTest, OmitsAbsentOptionalFieldsDuringSerialization) {
	auto typed = boost::json::value_to<user>(
	    boost::json::parse(R"({"name":"Alice","profile":{"display_name":"Alice A."},"addresses":[]})"));
	const auto serialized = boost::json::value_from(std::move(typed));
	const auto &obj = serialized.as_object();

	EXPECT_FALSE(obj.if_contains("nickname"));
	EXPECT_TRUE(obj.if_contains("profile"));
	EXPECT_TRUE(obj.if_contains("addresses"));
}

TEST(JsonObjectContractTest, ReportsMissingRequiredFieldsWithTypeName) {
	EXPECT_THROW(
	    {
		    try {
			    static_cast<void>(boost::json::value_to<user>(
			        boost::json::parse(R"({"profile":{"display_name":"Alice A."},"addresses":[]})")));
		    } catch (const std::invalid_argument &ex) {
			    EXPECT_STREQ(ex.what(), "missing required field 'name' for user");
			    throw;
		    }
	    },
	    std::invalid_argument);
}

TEST(JsonObjectContractTest, WrapsNestedTypeMismatchesWithFieldContext) {
	EXPECT_THROW(
	    {
		    try {
			    static_cast<void>(boost::json::value_to<user>(
			        boost::json::parse(R"({"name":"Alice","profile":{"display_name":42},"addresses":[]})")));
		    } catch (const std::invalid_argument &ex) {
			    const std::string message = ex.what();
			    EXPECT_NE(message.find("invalid field 'profile' for user"), std::string::npos);
			    EXPECT_NE(message.find("display_name"), std::string::npos);
			    throw;
		    }
	    },
	    std::invalid_argument);
}

TEST(JsonObjectContractTest, RejectsNonObjectsBeforeFieldParsing) {
	EXPECT_THROW(
	    {
		    try {
			    static_cast<void>(boost::json::value_to<user>(boost::json::parse(R"(["not","an","object"])")));
		    } catch (const std::invalid_argument &ex) {
			    EXPECT_STREQ(ex.what(), "expected JSON object for user");
			    throw;
		    }
	    },
	    std::invalid_argument);
}

TEST(JsonObjectContractTest, SkipsValidationUnlessExplicitlyRequested) {
	const auto parsed = warp::codegen::parse_json_object<validated_request>(
	    boost::json::parse(R"({"name":"Al","nickname":"toolong","age":12})"));

	EXPECT_EQ(parsed.name, "Al");
	ASSERT_TRUE(parsed.nickname.has_value());
	EXPECT_EQ(*parsed.nickname, "toolong");
	EXPECT_EQ(parsed.age, 12);
}

TEST(JsonObjectContractTest, EnforcesStringValidationWhenRequested) {
	EXPECT_THROW(
	    {
		    try {
			    static_cast<void>(warp::codegen::parse_json_object<validated_request>(
			        boost::json::parse(R"({"name":"Al","nickname":"ally","age":21})"),
			        warp::codegen::json_validation_mode::enforce));
		    } catch (const std::invalid_argument &ex) {
			    const std::string message = ex.what();
			    EXPECT_NE(message.find("invalid field 'name' for validated_request"), std::string::npos);
			    EXPECT_NE(message.find("length >= 3"), std::string::npos);
			    throw;
		    }
	    },
	    std::invalid_argument);
}

TEST(JsonObjectContractTest, EnforcesNumericValidationWhenRequested) {
	EXPECT_THROW(
	    {
		    try {
			    static_cast<void>(warp::codegen::parse_json_object<validated_request>(
			        boost::json::parse(R"({"name":"Alice","nickname":"ally","age":12})"),
			        warp::codegen::json_validation_mode::enforce));
		    } catch (const std::invalid_argument &ex) {
			    const std::string message = ex.what();
			    EXPECT_NE(message.find("invalid field 'age' for validated_request"), std::string::npos);
			    EXPECT_NE(message.find(">= 18"), std::string::npos);
			    throw;
		    }
	    },
	    std::invalid_argument);
}

TEST(JsonObjectContractTest, SerializationDoesNotApplyRequestValidation) {
	const auto serialized = boost::json::value_from(validated_request {.name = "Al", .nickname = "toolong", .age = 12});
	const auto &obj = serialized.as_object();

	EXPECT_EQ(std::string(obj.at("name").as_string()), "Al");
	EXPECT_EQ(std::string(obj.at("nickname").as_string()), "toolong");
	EXPECT_EQ(obj.at("age").as_int64(), 12);
}

} // namespace
