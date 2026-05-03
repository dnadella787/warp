//
// Created by Dhanush Nadella on 5/3/26.
//

#pragma once
#include <functional>
#include <memory>
#include <boost/asio/strand.hpp>
#include <boost/asio/io_context.hpp>
#include <boost/asio/system_timer.hpp>

#include "warp/server/job.hpp"

namespace asio = boost::asio;

namespace warp::server {

    struct erased_job {
        bool run() {
            return run_();
        }

        [[nodiscard]] warp::job::job_config config() const {
            return config_;
        }

        std::function<bool()> run_;
        warp::job::job_config config_;
    };

    template <warp::job::job Job>
    class job_wrapper : public std::enable_shared_from_this<job_wrapper<Job>> {
    public:
        job_wrapper(asio::io_context& io, Job job) : strand_(asio::make_strand(io)),
                timer_(strand_),
                job_(std::move(job)),
                config_(job_.config()) {}

        // return bool because we will emit metrics from these in the future
        bool operator()() {
            unsigned short attempts = 0;
            while (attempts <= config_.max_retries) {
                if (job_.run()) {
                    return true;
                }
                ++attempts;
            }
            return false;
        }

        void start() {
            if (config_.max_ttl == 0) {
                return;
            }

            schedule_after(config_.initial_delay_seconds);
        }
    private:
        void schedule_after(std::chrono::seconds delay) {
            timer_.expires_after(delay);
            auto self = this->shared_from_this();
            timer_.async_wait([self](const boost::system::error_code& ec) {
                if (ec) {
                    return;
                }

                if (self->remaining_runs_ == 0) {
                    return;
                }

                --self->remaining_runs_;
                (*self)();

                if (self->remaining_runs_ > 0) {
                    self->schedule_after(self->config_.interval);
                }
            });
        }

        Job job_;
        warp::job::job_config config_;
        asio::strand<asio::io_context::executor_type> strand_;
        asio::system_timer timer_;
        unsigned short remaining_runs_ {config_.max_ttl};
    };

}
