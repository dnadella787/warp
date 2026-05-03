#pragma once

#include <concepts>
#include <string>
#include <type_traits>

namespace warp::ssl {

class cert_loader {
public:
	virtual ~cert_loader() = default;

	[[nodiscard]] virtual std::string load_pem_bundle() const = 0;

protected:
	cert_loader() = default;
};

template <typename Loader>
concept pem_bundle_cert_loader =
    std::derived_from<std::remove_cvref_t<Loader>, cert_loader> && requires(const std::remove_cvref_t<Loader> &loader) {
	    { loader.load_pem_bundle() } -> std::same_as<std::string>;
    };

} // namespace warp::ssl
