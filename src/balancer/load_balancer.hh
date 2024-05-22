#pragma once

#include <seastar/core/smp.hh>

namespace amadeus {

using namespace seastar;

class load_balancer {
    std::mutex _lock;
    shard_id _next = 0;

 public:
    load_balancer() {}

    shard_id next_cpu() {
        std::lock_guard<std::mutex> guard(_lock);

        auto next = _next;
        _next = (_next + 1) % smp::count;

        return next;
    }

    static load_balancer global;
};

} // namespace amadeus
