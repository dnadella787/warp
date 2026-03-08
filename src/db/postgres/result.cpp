#include "warp/db/postgres/result.hpp"

#include <pqxx/pqxx>

#include <stdexcept>

namespace warp::db::postgres {

result::result() = default;

result::result(std::shared_ptr<pqxx::result> data) : data_(std::move(data)) {
}

result::result(const result &) = default;
result::result(result &&) noexcept = default;
result &result::operator=(const result &) = default;
result &result::operator=(result &&) noexcept = default;
result::~result() = default;

int result::rows() const {
	return data_ ? static_cast<int>(data_->size()) : 0;
}

int result::columns() const {
	return data_ ? static_cast<int>(data_->columns()) : 0;
}

std::string_view result::column_name(int index) const {
	if (!data_) {
		throw std::logic_error("result is empty");
	}
	const char *name = data_->column_name(index);
	if (!name) {
		throw std::out_of_range("column index out of range");
	}
	return std::string_view {name};
}

std::string_view result::value(int row, int column) const {
	if (!data_) {
		throw std::logic_error("result is empty");
	}
	if (row < 0 || column < 0) {
		throw std::out_of_range("row or column index is negative");
	}
	if (row >= rows() || column >= columns()) {
		throw std::out_of_range("row or column index out of range");
	}
	const auto field = (*data_)[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)];
	if (field.is_null()) {
		return {};
	}
	return std::string_view {field.c_str()};
}

bool result::value_is_null(int row, int column) const {
	if (!data_) {
		return true;
	}
	if (row < 0 || column < 0) {
		throw std::out_of_range("row or column index is negative");
	}
	if (row >= rows() || column >= columns()) {
		throw std::out_of_range("row or column index out of range");
	}
	return (*data_)[static_cast<std::size_t>(row)][static_cast<std::size_t>(column)].is_null();
}

} // namespace warp::db::postgres
