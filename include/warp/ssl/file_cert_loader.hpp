#pragma once

#include <filesystem>
#include <fstream>
#include <iterator>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>

#include "warp/ssl/cert_loader.hpp"

namespace warp::ssl {

class file_cert_loader : public cert_loader {
public:
	explicit file_cert_loader(std::string pem_bundle_path) : pem_bundle_path_(std::move(pem_bundle_path)) {
	}

	[[nodiscard]] bool have_certs_changed() const override {
		const auto current_write_time = last_write_time();
		if (!last_loaded_write_time_) {
			last_loaded_write_time_ = current_write_time;
			return true;
		}

		if (*last_loaded_write_time_ == current_write_time) {
			return false;
		}

		last_loaded_write_time_ = current_write_time;
		return true;
	}

	[[nodiscard]] std::string load_pem_bundle() const override {
		const auto current_write_time = last_write_time();
		std::ifstream input(pem_bundle_path_, std::ios::binary);
		if (!input) {
			throw std::runtime_error("failed to open TLS PEM bundle: " + pem_bundle_path_);
		}

		auto pem_bundle = std::string(std::istreambuf_iterator(input), std::istreambuf_iterator<char>());
		last_loaded_write_time_ = current_write_time;
		return pem_bundle;
	}

	[[nodiscard]] const std::string &pem_bundle_path() const noexcept {
		return pem_bundle_path_;
	}

private:
	[[nodiscard]] std::filesystem::file_time_type last_write_time() const {
		std::error_code ec;
		const auto write_time = std::filesystem::last_write_time(pem_bundle_path_, ec);
		if (ec) {
			throw std::runtime_error("failed to stat TLS PEM bundle: " + pem_bundle_path_);
		}
		return write_time;
	}

	std::string pem_bundle_path_;
	mutable std::optional<std::filesystem::file_time_type> last_loaded_write_time_;
};

} // namespace warp::ssl
