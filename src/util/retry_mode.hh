#pragma once

#include <seastar/core/seastar.hh>

namespace amadeus {
namespace util {

using namespace seastar;

class retry_exception : public std::runtime_error {
 public:
    retry_exception(const sstring &s)
    : std::runtime_error(s) {}

    virtual ~retry_exception() = default;
};

class retry_runner;

class retry_mode {
 public:
    retry_mode() = default;
    virtual ~retry_mode() = default;

    virtual future<> do_try();

    int max_try_times() const {
        return _max_try_times;
    }

    void set_max_try_times(int max_try_times) {
        _max_try_times = max_try_times;
    }

    int try_times() const {
        return _try_times;
    }

    int retry_times() const {
        return _try_times - 1;
    }

    std::chrono::system_clock::time_point timestamp() const {
        return _timestamp;
    }

    virtual void cancel() {}

 protected:
    friend class retry_runner;
    virtual void _reset();
    virtual void _on_try_once();

    virtual future<bool> _validate();
    virtual future<> _do_try();

    bool _excuting = false; //是否正在执行
    int _try_times = 0; //尝试的次数
    int _total_try_times = 0; //预计尝试的次数
    int _max_try_times = 1; //最大尝试次数

    std::function<future<>(int, int)> _func; //执行函数

    std::chrono::system_clock::time_point _timestamp; 
    std::chrono::system_clock::time_point _last_timestamp;
    std::chrono::system_clock::time_point _first_timestamp;
};

class delay_retry_mode : public retry_mode {
 public:
    delay_retry_mode() = default;
    virtual ~delay_retry_mode() = default;

    std::chrono::milliseconds delay() const {
        return _delay;
    }

    void set_delay(std::chrono::milliseconds delay) {
        _delay = delay;
    }

    virtual void cancel() override {
        if (_cancel_pr != std::nullopt) _cancel_pr->set_value();
    }

 protected:
    virtual future<> _do_try() override;

    virtual std::chrono::milliseconds current_delay() const {
        return _delay;
    }

    std::chrono::milliseconds _delay;
    std::optional<promise<>> _cancel_pr = std::nullopt;
};

class delay_exponent_retry_mode : public delay_retry_mode {
 public:
    delay_exponent_retry_mode() = default;
    virtual ~delay_exponent_retry_mode() = default;

 protected:
    virtual std::chrono::milliseconds current_delay() const override;
};

class timeout_retry_mode : public retry_mode {
 public:
    timeout_retry_mode() = default;
    virtual ~timeout_retry_mode() = default;

    std::chrono::milliseconds timeout_duration() const {
        return _timeout_duration;
    }

    void set_timeout_duration(std::chrono::milliseconds timeout_duration) {
        _timeout_duration = timeout_duration;
    }

    virtual void cancel() override {
        if (_cancel_pr != std::nullopt) _cancel_pr->set_value();
    }

 protected:
    virtual future<bool> _validate() override;
    virtual future<> _do_try() override;

    virtual std::chrono::milliseconds current_timeout_duration() const {
        return _timeout_duration;
    }

    bool _sleeping = false;
    std::chrono::milliseconds _timeout_duration;
    std::optional<promise<>> _cancel_pr;
};

class timeout_exponent_retry_mode : public timeout_retry_mode {
 public:
    timeout_exponent_retry_mode() = default;
    virtual ~timeout_exponent_retry_mode() = default;

 protected:
    virtual std::chrono::milliseconds current_timeout_duration() const override;
};

} // namespace util
} // namespace amadeus
