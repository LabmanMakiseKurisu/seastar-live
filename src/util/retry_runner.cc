/*
 * @Author: Amadeus
 * @Date: 2024-04-22 16:00:01
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-22 20:09:19
 * @FilePath: /Amadeus/src/util/retry_runner.cc
 * @Description: 
 */
#include "util/retry_runner.hh"
#include <util/CxxUrl.hh>
#include <fmt/printf.h>
#include <seastar/core/loop.hh>
#include <seastar/core/sstring.hh>
#include <seastar/net/socket_defs.hh>
#include <seastar/util/log.hh>
#include "retry_runner.hh"

namespace amadeus {
namespace util {

using namespace seastar;
using namespace std::literals::chrono_literals;

void
retry_runner::init() {
    mode()->_func = [this](int times, int total_times) {
        _keep_alive = times < mode()->max_try_times();

        return try_once(times, total_times);
    };
}

void
retry_runner::finished() {
    _finished = true;

    on_retry_finished();
}

void
retry_runner::cancel() {
    _finished = true;

    mode()->cancel();
}

future<>
retry_runner::run() {
    return do_until(
               [this] {
                   return _finished;
               },
               [this] {
                   return do_try()
                       .then([this] {
                           finished();
                       })
                       .handle_exception_type([this](retry_exception e) {
                           finished();
                       })
                       .handle_exception([this](auto e) {
                           // ignored exception
                       });
               })
        .then([this] {
            if (_finished) on_retry_finished();
        });
}

future<>
retry_runner::do_try() {
    return mode()->do_try();
}

delay_retry_runner::delay_retry_runner(std::unique_ptr<delay_retry_mode> mode)
: _retry_mode(std::move(mode)) {
    init();
}

timeout_retry_runner::timeout_retry_runner(std::unique_ptr<timeout_retry_mode> mode)
: _retry_mode(std::move(mode)) {
    init();
}

} // namespace util
} // namespace amadeus
