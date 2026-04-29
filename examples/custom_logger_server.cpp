#include "warp/server/server.hpp"

#include <string>

#include "warp/server/server_builder.hpp"
#include "warp/warp.hpp"

int main() {
	// `true` truncates the file on startup so each example run starts with a clean log.
	auto combined_logger =
	    warp::log::logger("warp.custom_logger_server", {warp::log::sink::stdout_color(),
	                                                    warp::log::sink::basic_file("warp-custom-server.log", true)});
	combined_logger.set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%n] %v");
	combined_logger.set_level(warp::log::level::info);
	// File sinks are buffered; flush on info so tailing the file shows requests before shutdown.
	combined_logger.flush_on(warp::log::level::info);

	// App-level logs use the process default logger.
	combined_logger.set_as_default();

	auto server = warp::server::server_builder()
	                  .address("127.0.0.1")
	                  .port(8080)
	                  .worker_threads(2)
	                  // build() captures this logger for Warp's internal listener/session/server logs.
	                  .logger(combined_logger)
	                  .get<"/hello/{name}">([](const warp::request &req) -> warp::response {
		                  auto name = req.path_param("name").value_or("world");
		                  warp::log::info("Received request for /hello/{}", name);
		                  return warp::response::ok(warp::body_builder().set("name", std::string(name)).build());
	                  })
	                  .get<"/ping">([](const warp::request &) -> warp::response {
		                  warp::log::info("Received request for /ping");
		                  return warp::response::ok(warp::body_builder().set("name", "pong").build());
	                  })
	                  .build<warp::event_loop_mode::coroutines>();

	warp::log::info("Custom logger example server listening on http://127.0.0.1:8080");
	warp::log::info("Logs will be written to stdout and warp-custom-server.log with info-level flushing enabled");
	server.run();
	return 0;
}
