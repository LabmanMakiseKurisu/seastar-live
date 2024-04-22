/*
 * @Author: Amadeus
 * @Date: 2024-04-22 16:00:01
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-22 20:12:17
 * @FilePath: /Amadeus/src/util/retry_mode.cc
 * @Description: 
 */
#include "util/retry_mode.hh"

#include <seastar/core/with_timeout.hh>

#include <cmath>

namespace amadeus {
namespace util {

using namespace seastar;

future<>
retry_mode::do_try() {
    return _validate().then([this](bool valid) {
        if (!valid) return make_ready_future<>();

        return _do_try();
    });
}

future<>
retry_mode::_do_try() {
    ++_try_times;
    ++_total_try_times;

    _excuting = true;
    _timestamp = std::chrono::system_clock::now();
    return _func(_try_times, _total_try_times).then_wrapped([this](auto f) {
        _excuting = false;

        if (f.failed()) {
            _on_try_once();
        } else {
            _reset();
        }
        return std::move(f);
    });
}

void
retry_mode::_reset() {
    auto now = std::chrono::system_clock::now();

    _try_times = 0;
    _last_timestamp = now;
    _first_timestamp = now;
}

future<bool>
retry_mode::_validate() {
    if (_excuting) return make_ready_future<bool>(false);
    if (_try_times >= _max_try_times) return make_exception_future<bool>(retry_exception("times out of range"));

    return make_ready_future<bool>(true);
}

void
retry_mode::_on_try_once() {
    auto now = std::chrono::system_clock::now();

    bool first_time = _try_times == 1;
    if (first_time) _first_timestamp = now;

    _last_timestamp = now;
}

future<>
delay_retry_mode::_do_try() {
    if (_try_times == 0) return retry_mode::_do_try();

    auto delay = current_delay();
    auto tp = std::chrono::steady_clock::now() + delay;

    _cancel_pr = std::make_optional(promise<>());
    return with_timeout(tp, _cancel_pr->get_future()).handle_exception_type([this](timed_out_error e) {
        _cancel_pr = std::nullopt;
        return retry_mode::_do_try();
    });
}

std::chrono::milliseconds
delay_exponent_retry_mode::current_delay() const {
    auto delay = _delay;
    delay *= std::pow(2, _try_times);

    return delay;
}

future<bool>
timeout_retry_mode::_validate() {
    if (_sleeping) return make_ready_future<bool>(false);

    return retry_mode::_validate();
}

future<>
timeout_retry_mode::_do_try() {
    if (_sleeping) return make_ready_future<>();

    auto now = std::chrono::system_clock::now();
    auto timeout_duration = current_timeout_duration();

    auto time_offset = now - _last_timestamp;
    bool is_timeout = time_offset > timeout_duration;

    if (is_timeout) {
        return retry_mode::_do_try();
    } else {
        auto delta = timeout_duration - time_offset;
        auto tp = std::chrono::steady_clock::now() + delta;

        _sleeping = true;
        _cancel_pr = std::make_optional(promise<>());
        return with_timeout(tp, _cancel_pr->get_future()).handle_exception_type([this](timed_out_error e) {
            _sleeping = false;
            _cancel_pr = std::nullopt;
            return retry_mode::_do_try();
        });
    }
}

std::chrono::milliseconds
timeout_exponent_retry_mode::current_timeout_duration() const {
    auto timeout_duration = _timeout_duration;
    timeout_duration *= std::pow(2, _try_times);

    return timeout_duration;
}

} // namespace util
} // namespace amadeus
