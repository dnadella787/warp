#pragma once

#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string>
#include <utility>

#include "warp/ssl/cert_loader.hpp"

namespace warp::ssl {

class file_cert_loader : public cert_loader {
public:
	explicit file_cert_loader(std::string pem_bundle_path) : pem_bundle_path_(std::move(pem_bundle_path)) {
	}

	[[nodiscard]] std::string load_pem_bundle() const {
		std::ifstream input(pem_bundle_path_, std::ios::binary);
		if (!input) {
			throw std::runtime_error("failed to open TLS PEM bundle: " + pem_bundle_path_);
		}

		return {std::istreambuf_iterator(input), std::istreambuf_iterator<char>()};
	}

	[[nodiscard]] const std::string &pem_bundle_path() const noexcept {
		return pem_bundle_path_;
	}

private:
	std::string pem_bundle_path_;
};

} // namespace warp::ssl
