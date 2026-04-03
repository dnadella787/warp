#include "warp/codegen/generator.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string_view>

namespace {

void write_file_atomically(const std::filesystem::path &path, std::string_view content) {
	const auto temp_path =
	    path.parent_path() / (path.filename().string() + ".tmp." +
	                          std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));

	std::ofstream output(temp_path);
	if (!output.is_open()) {
		throw std::runtime_error("failed to open output file: " + temp_path.string());
	}
	output << content;
	if (!output.good()) {
		throw std::runtime_error("failed to write output file: " + temp_path.string());
	}
	output.close();
	std::filesystem::rename(temp_path, path);
}

} // namespace

int main(int argc, char **argv) {
	if (argc < 2 || argc > 3) {
		std::cerr << "usage: warp_example_generate_users_headers <yaml-path> [output-dir]\n";
		return 1;
	}

	const std::filesystem::path yaml_path(argv[1]);
	const auto output_dir = argc == 3 ? std::filesystem::path(argv[2]) : std::filesystem::current_path();
	std::filesystem::create_directories(output_dir);
	const auto generated = warp::codegen::api_stub_generator().generate_from_file(
	    yaml_path, {
	                   .model_header_name = "generated_api_types.hpp",
	                   .resource_header_name = "generated_api_resources.hpp",
	               });

	write_file_atomically(output_dir / "generated_api_types.hpp", generated.model_header);
	write_file_atomically(output_dir / "generated_api_resources.hpp", generated.resource_header);
	std::cout << "Wrote generated_api_types.hpp and generated_api_resources.hpp to " << output_dir << '\n';
	return 0;
}
