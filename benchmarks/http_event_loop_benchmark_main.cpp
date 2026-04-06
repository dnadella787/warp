#include <fstream>
#include <benchmark/benchmark.h>

#include "http_event_loop_benchmark_support.hpp"

#include <iostream>
#include <string>

namespace warp::benchmarks {

void register_round_trip_benchmarks();
#if defined(WARP_BENCHMARK_HAS_DB)
void register_db_round_trip_benchmarks();
#endif

} // namespace warp::benchmarks

int main(int argc, char **argv) {
	std::string error_message;
	if (!warp::benchmarks::parse_load_test_arguments(argc, argv, error_message)) {
		std::cerr << error_message << '\n';
		return 1;
	}

	warp::benchmarks::register_round_trip_benchmarks();
#if defined(WARP_BENCHMARK_HAS_DB)
	warp::benchmarks::register_db_round_trip_benchmarks();
#endif

	benchmark::Initialize(&argc, argv);
	if (benchmark::ReportUnrecognizedArguments(argc, argv)) {
		return 1;
	}

	warp::benchmarks::add_load_test_runtime_context();

	benchmark::RunSpecifiedBenchmarks();
	benchmark::Shutdown();
	return 0;
}
