#pragma once

#include <memory>
#include <string_view>

namespace pqxx {
class result;
} // namespace pqxx

namespace warp::db::postgres {

class result {
public:
	result();
	explicit result(std::shared_ptr<pqxx::result> data);
	result(const result &);
	result(result &&) noexcept;
	result &operator=(const result &);
	result &operator=(result &&) noexcept;
	~result();

	[[nodiscard]] int rows() const;
	[[nodiscard]] int columns() const;
	[[nodiscard]] std::string_view column_name(int index) const;
	[[nodiscard]] std::string_view value(int row, int column) const;
	[[nodiscard]] bool value_is_null(int row, int column) const;

private:
	std::shared_ptr<pqxx::result> data_;
};

} // namespace warp::db::postgres
