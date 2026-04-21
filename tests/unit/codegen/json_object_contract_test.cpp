#include "warp/codegen/json_object_contract.hpp"

#include <gtest/gtest.h>

#include <boost/json/parse.hpp>
#include <boost/json/serialize.hpp>

#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace json_contract_test_model {

class address {
public:
	[[nodiscard]] const std::string &city() const & noexcept {
		return city_;
	}

	[[nodiscard]] std::string &&city() && noexcept {
		return std::move(city_);
	}

	address &set_city(std::string value) {
		city_ = std::move(value);
		return *this;
	}

private:
	std::string city_ {};
};

class profile {
public:
	[[nodiscard]] const std::string &display_name() const & noexcept {
		return display_name_;
	}

	[[nodiscard]] std::string &&display_name() && noexcept {
		return std::move(display_name_);
	}

	profile &set_display_name(std::string value) {
		display_name_ = std::move(value);
		return *this;
	}

private:
	std::string display_name_ {};
};

class user {
public:
	[[nodiscard]] const std::string &name() const & noexcept {
		return name_;
	}

	[[nodiscard]] std::string &&name() && noexcept {
		return std::move(name_);
	}

	user &set_name(std::string value) {
		name_ = std::move(value);
		return *this;
	}

	[[nodiscard]] const std::optional<std::string> &nickname() const & noexcept {
		return nickname_;
	}

	[[nodiscard]] std::optional<std::string> &&nickname() && noexcept {
		return std::move(nickname_);
	}

	user &set_nickname(std::optional<std::string> value) {
		nickname_ = std::move(value);
		return *this;
	}

	[[nodiscard]] const profile &profile_data() const & noexcept {
		return profile_data_;
	}

	[[nodiscard]] profile &&profile_data() && noexcept {
		return std::move(profile_data_);
	}

	user &set_profile_data(profile value) {
		profile_data_ = std::move(value);
		return *this;
	}

	[[nodiscard]] const std::vector<address> &addresses() const & noexcept {
		return addresses_;
	}

	[[nodiscard]] std::vector<address> &&addresses() && noexcept {
		return std::move(addresses_);
	}

	user &set_addresses(std::vector<address> value) {
		addresses_ = std::move(value);
		return *this;
	}

private:
	std::string name_ {};
	std::optional<std::string> nickname_ {};
	profile profile_data_ {};
	std::vector<address> addresses_ {};
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
	static constexpr auto fields = std::make_tuple(make_required_json_field(
	    "city",
	    static_cast<json_contract_test_model::address &(json_contract_test_model::address::*)(std::string)>(
	        &json_contract_test_model::address::set_city),
	    static_cast<const std::string &(json_contract_test_model::address::*)() const & noexcept>(
	        &json_contract_test_model::address::city),
	    static_cast<std::string && (json_contract_test_model::address::*)() && noexcept>(
	        &json_contract_test_model::address::city)));
};

template <>
struct json_object_contract<json_contract_test_model::profile> {
	static constexpr std::string_view type_name = "profile";
	static constexpr auto fields = std::make_tuple(make_required_json_field(
	    "display_name",
	    static_cast<json_contract_test_model::profile &(json_contract_test_model::profile::*)(std::string)>(
	        &json_contract_test_model::profile::set_display_name),
	    static_cast<const std::string &(json_contract_test_model::profile::*)() const & noexcept>(
	        &json_contract_test_model::profile::display_name),
	    static_cast<std::string && (json_contract_test_model::profile::*)() && noexcept>(
	        &json_contract_test_model::profile::display_name)));
};

template <>
struct json_object_contract<json_contract_test_model::user> {
	static constexpr std::string_view type_name = "user";
	static constexpr auto fields = std::make_tuple(
	    make_required_json_field(
	        "name",
	        static_cast<json_contract_test_model::user &(json_contract_test_model::user::*)(std::string)>(
	            &json_contract_test_model::user::set_name),
	        static_cast<const std::string &(json_contract_test_model::user::*)() const & noexcept>(
	            &json_contract_test_model::user::name),
	        static_cast<std::string && (json_contract_test_model::user::*)() && noexcept>(
	            &json_contract_test_model::user::name)),
	    make_optional_json_field(
	        "nickname",
	        static_cast<json_contract_test_model::user &(
	            json_contract_test_model::user::*)(std::optional<std::string>)>(
	            &json_contract_test_model::user::set_nickname),
	        static_cast<const std::optional<std::string> &(json_contract_test_model::user::*)() const & noexcept>(
	            &json_contract_test_model::user::nickname),
	        static_cast<std::optional<std::string> && (json_contract_test_model::user::*)() && noexcept>(
	            &json_contract_test_model::user::nickname)),
	    make_required_json_field(
	        "profile",
	        static_cast<json_contract_test_model::user &(
	            json_contract_test_model::user::*)(json_contract_test_model::profile)>(
	            &json_contract_test_model::user::set_profile_data),
	        static_cast<const json_contract_test_model::profile &(json_contract_test_model::user::*)() const &
	                    noexcept>(&json_contract_test_model::user::profile_data),
	        static_cast<json_contract_test_model::profile && (json_contract_test_model::user::*)() && noexcept>(
	            &json_contract_test_model::user::profile_data)),
	    make_required_json_field(
	        "addresses",
	        static_cast<json_contract_test_model::user &(
	            json_contract_test_model::user::*)(std::vector<json_contract_test_model::address>)>(
	            &json_contract_test_model::user::set_addresses),
	        static_cast<const std::vector<json_contract_test_model::address> &(json_contract_test_model::user::*)()
	                        const &
	                    noexcept>(&json_contract_test_model::user::addresses),
	        static_cast<std::vector<json_contract_test_model::address> && (json_contract_test_model::user::*)() &&
	                    noexcept>(&json_contract_test_model::user::addresses)));
};

} // namespace warp::codegen

namespace {

using json_contract_test_model::user;

TEST(JsonObjectContractTest, ParsesAndSerializesNestedMetadataDrivenObjects) {
	const auto parsed = boost::json::parse(
	    R"({"name":"Alice","nickname":"ally","profile":{"display_name":"Alice A."},"addresses":[{"city":"Austin"},{"city":"Dallas"}]})");
	const auto typed = boost::json::value_to<user>(parsed);

	EXPECT_EQ(typed.name(), "Alice");
	ASSERT_TRUE(typed.nickname().has_value());
	EXPECT_EQ(*typed.nickname(), "ally");
	EXPECT_EQ(typed.profile_data().display_name(), "Alice A.");
	ASSERT_EQ(typed.addresses().size(), 2U);
	EXPECT_EQ(typed.addresses()[0].city(), "Austin");
	EXPECT_EQ(typed.addresses()[1].city(), "Dallas");

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

} // namespace
