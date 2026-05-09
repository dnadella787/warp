#include "warp/codegen/generator.hpp"

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace {

struct cli_options {
	std::filesystem::path spec_path;
	std::filesystem::path output_dir;
	std::string namespace_name;
	std::string model_header_name {"generated_api_types.hpp"};
	std::string resource_header_name {"generated_api_resources.hpp"};
};

[[nodiscard]] std::string usage() {
	return "usage: warp_codegen --spec <yaml-path> --output-dir <dir> "
	       "[--namespace <cpp-namespace>] "
	       "[--model-header <filename>] "
	       "[--resource-header <filename>]";
}

void require_value(int argc, int index, std::string_view option) {
	if (index + 1 >= argc) {
		throw std::invalid_argument("missing value for " + std::string(option));
	}
}

[[nodiscard]] cli_options parse_args(int argc, char **argv) {
	cli_options options;

	for (int index = 1; index < argc; ++index) {
		const std::string_view arg(argv[index]);
		if (arg == "--help" || arg == "-h") {
			std::cout << usage() << '\n';
			std::exit(0);
		}
		if (arg == "--spec") {
			require_value(argc, index, "--spec");
			options.spec_path = argv[++index];
			continue;
		}
		if (arg == "--output-dir") {
			require_value(argc, index, "--output-dir");
			options.output_dir = argv[++index];
			continue;
		}
		if (arg == "--namespace") {
			require_value(argc, index, "--namespace");
			options.namespace_name = argv[++index];
			continue;
		}
		if (arg == "--model-header") {
			require_value(argc, index, "--model-header");
			options.model_header_name = argv[++index];
			continue;
		}
		if (arg == "--resource-header") {
			require_value(argc, index, "--resource-header");
			options.resource_header_name = argv[++index];
			continue;
		}
		throw std::invalid_argument("unknown argument: " + std::string(arg));
	}

	if (options.spec_path.empty()) {
		throw std::invalid_argument("--spec is required");
	}
	if (options.output_dir.empty()) {
		throw std::invalid_argument("--output-dir is required");
	}
	if (options.model_header_name.empty()) {
		throw std::invalid_argument("--model-header cannot be empty");
	}
	if (options.resource_header_name.empty()) {
		throw std::invalid_argument("--resource-header cannot be empty");
	}

	return options;
}

// write to a temp file first and then update that file to the actual locatioin
void write_file_atomically(const std::filesystem::path &path, std::string_view content) {
	if (!path.parent_path().empty()) {
		std::filesystem::create_directories(path.parent_path());
	}

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
	try {
		const auto options = parse_args(argc, argv);

		std::filesystem::create_directories(options.output_dir);
		const auto generated = warp::codegen::api_stub_generator().generate_from_file(
		    options.spec_path, {
		                           .namespace_name = options.namespace_name,
		                           .model_header_name = options.model_header_name,
		                           .resource_header_name = options.resource_header_name,
		                       });

		write_file_atomically(options.output_dir / options.model_header_name, generated.model_header);
		write_file_atomically(options.output_dir / options.resource_header_name, generated.resource_header);
		return 0;
	} catch (const std::exception &ex) {
		std::cerr << "warp_codegen: " << ex.what() << '\n';
		std::cerr << usage() << '\n';
		return 1;
	}
}
