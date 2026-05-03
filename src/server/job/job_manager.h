//
// Created by Dhanush Nadella on 5/3/26.
//

#pragma once
#include <functional>
#include <memory>
#include <vector>
#include <boost/asio/io_context.hpp>

#include "job.h"

namespace asio = boost::asio;

namespace warp::server {

class job_manager {
public:
    job_manager() = delete;
    job_manager(asio::io_context& io) : io_context_(io) {}

    template <warp::job::job T>
    void add_job(T job) {
        auto wrapped_job = std::make_shared<job_wrapper<T>>(io_context_, std::move(job));
        jobs.emplace_back([wrapped_job]() {
            wrapped_job->start();
        });
    }

    void add_job(warp::job::background_job job) {
        add_job(erased_job {.run_ = std::move(job.run), .config_ = job.config});
    }

    void start_jobs() const {
        for (auto& j : jobs) {
            j();
        }
    }

private:
    asio::io_context& io_context_;
    std::vector<std::function<void()>> jobs;
};

}
