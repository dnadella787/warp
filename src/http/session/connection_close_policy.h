//
// Created by Dhanush Nadella on 4/15/26.
//

#pragma once
#include <cstddef>

#include "warp/warp.hpp"

namespace warp::http {
    struct request_context {
        std::size_t sequence {};
        unsigned version {11};
        bool client_keep_alive {true};
    };

    struct pending_write {
        warp::response response;
        bool close_after_write {false};
    };

    struct response_commit {
        bool drop_response {false};
        bool close_after_write {false};
    };

    class connection_close_policy {
    public:
        void on_request_accepted(std::size_t sequence, bool client_keep_alive) {
            // client sent connection close, set the close marker to this request as last
            if (!client_keep_alive)
                close_after(sequence);
        }

        response_commit on_response_ready(const request_context &req_ctx, warp::response &resp) {
            // this request comes after close marker, don't write it out, socket should be already closed
            if (close_after_sequence_.has_value() && req_ctx.sequence > close_after_sequence_.value())
                return response_commit{.drop_response = true, .close_after_write = false};

            // server wants to kill the connection now, try if the request hasn't already
            if (!resp.keep_alive())
                close_after(req_ctx.sequence);

            // if seq < last_then_close -> write it out
            // if seq > last_then_close -> socket already closed, we drop the response
            // if seq == last_then_close -> write it out, then close the socket
            const bool keep_alive = !close_after_sequence_.has_value() || req_ctx.sequence != close_after_sequence_.value();
            resp.version(req_ctx.version);
            resp.keep_alive(req_ctx.client_keep_alive && keep_alive);

            return response_commit {
                .drop_response = false,
                .close_after_write = !keep_alive,
            };
        }

        [[nodiscard]] bool accepting_requests() const noexcept {
            return accepting_requests_;
        }
    private:
        void close_after(const std::size_t &sequence) {
            accepting_requests_ = false;

            // this is the first close, set the marker, or
            // a previous close exists but it is for a later request so now we need to move the
            // close marker up so any requests between the old and new markers are also not sent.
            if (!close_after_sequence_.has_value() || sequence < close_after_sequence_.value())
                close_after_sequence_ = sequence;
        }

        bool accepting_requests_ {true};
        std::optional<std::size_t> close_after_sequence_;
    };
}
