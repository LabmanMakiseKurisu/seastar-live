#pragma once

#include <seastar/core/condition-variable.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/sharded.hh>

/// Seastar apps lib namespace

namespace seastar_apps_lib {

/// \brief Futurized SIGINT/SIGTERM signals handler class
///
/// Seastar-style helper class that allows easy waiting for SIGINT/SIGTERM signals
/// from your app.
///
/// Example:
/// \code
/// #include <seastar/apps/lib/stop_signal.hh>
/// ...
/// int main() {
/// ...
/// thread th([] {
///    seastar_apps_lib::stop_signal stop_signal;
///    <some code>
///    stop_signal.wait().get();  // this will wait till we receive SIGINT or SIGTERM signal
/// });
/// \endcode
class stop_signal {
    bool _caught = false;
    condition_variable _cond;

 private:
    void signaled() {
        if (_caught) { return; }
        _caught = true;
        _cond.broadcast();
    }

 public:
    stop_signal() {
        engine().handle_signal(SIGINT, [this] {
            signaled();
        });
        engine().handle_signal(SIGTERM, [this] {
            signaled();
        });
    }

    ~stop_signal() {
        // There's no way to unregister a handler yet, so register a no-op handler instead.
        engine().handle_signal(SIGINT, [] {});
        engine().handle_signal(SIGTERM, [] {});
    }

    future<> wait() {
        return _cond.wait([this] {
            return _caught;
        });
    }

    bool stopping() const {
        return _caught;
    }
};

} // namespace seastar_apps_lib
