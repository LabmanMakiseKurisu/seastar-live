#pragma once

#include <map>

#include "rtmp/handlers.hh"

namespace amadeus {
namespace rtmp {

using namespace seastar;

// route决定了数据或者请求的传输路径和处理路径
// 在此处，主要决定了不同的请求或者异常对应的handler
class routes {
 public:
    using exception_handler_fun = std::function<std::unique_ptr<rtmp::reply>(std::exception_ptr eptr)>;
    using exception_handler_id = size_t;

 private:
    handler_base* _default_handler = nullptr;                          // 默认handler
    handler_base* _handlers[request::mode::NUM_MODE] = {0};            // 各种request的handler
    exception_handler_fun _general_handler;                            // 异常的默认handler
    std::map<exception_handler_id, exception_handler_fun> _exceptions; // 各种异常的handler
    exception_handler_id _exception_id = 0; //当前_exceptions中存着的异常handler数

 public:
    routes();
    ~routes();
    //设置m模式的handler
    routes& put(request::mode m, handler_base* handler) {
        if (_handlers[m]) throw std::runtime_error(format("Handler for {} already exists.", m));

        _handlers[m] = handler;
        return *this;
    }
    //强制设置m模式的handler
    routes& set(request::mode m, handler_base* handler) {
        _handlers[m] = handler;
        return *this;
    }
    //解除m模式的handler
    handler_base* drop(request::mode m) {
        auto handler = _handlers[m];
        _handlers[m] = nullptr;

        return handler;
    }
    //设置默认的handler
    routes& set_default_handler(handler_base* handler) {
        _default_handler = handler;
        return *this;
    }
    //处理请求
    future<std::unique_ptr<rtmp::reply>>
    handle(request::mode m, std::unique_ptr<request>& req, std::unique_ptr<rtmp::reply> rep);

    handler_base* get_handler(request::mode m) {
        handler_base* handler = _handlers[m];
        if (handler != nullptr) return handler;

        return _default_handler;
    }

    exception_handler_id register_exeption_handler(exception_handler_fun fun) {
        auto current = _exception_id++;
        _exceptions[current] = fun;
        return current;
    }

    void remove_exception_handler(exception_handler_id id) {
        _exceptions.erase(id);
    }

    std::unique_ptr<rtmp::reply> exception_reply(std::exception_ptr eptr);
};

} // namespace rtmp
} // namespace amadeus