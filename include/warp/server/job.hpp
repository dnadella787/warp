#pragma once

#include <chrono>
#include <concepts>
#include <functional>
#include <utility>

namespace warp::job {

struct job_config {
	std::chrono::seconds initial_delay_seconds {60};
	std::chrono::seconds interval {30};
	unsigned short max_retries {3};
	unsigned short max_ttl {10};
};

template <typename T>
concept job = requires(T candidate) {
	{ candidate.run() } -> std::same_as<bool>;
	{ candidate.config() } -> std::same_as<job_config>;
};

struct background_job {
	std::function<bool()> run;
	job_config config;
};

template <job Job>
[[nodiscard]] background_job make_background_job(Job job) {
	return background_job {
	    .run = [job = std::move(job)]() mutable -> bool { return job.run(); },
	    .config = job.config(),
	};
}

} // namespace warp::job
