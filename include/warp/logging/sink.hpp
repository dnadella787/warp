#pragma once

#include <memory>
#include <string>

namespace warp::log {

class logger;

class sink {
public:
	~sink();
	sink(const sink &);
	sink(sink &&) noexcept;
	sink &operator=(const sink &);
	sink &operator=(sink &&) noexcept;

	[[nodiscard]] static sink stderr_color();
	[[nodiscard]] static sink stdout_color();
	[[nodiscard]] static sink basic_file(std::string filename, bool truncate = false);

	[[nodiscard]] explicit operator bool() const noexcept;

private:
	friend class logger;
	struct impl;

	explicit sink(std::shared_ptr<impl> impl) noexcept;

	std::shared_ptr<impl> impl_;
};

} // namespace warp::log
