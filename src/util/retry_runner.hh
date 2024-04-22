/*
 * This file is open source software, licensed to you under the terms
 * of the Apache License, Version 2.0 (the "License").  See the NOTICE file
 * distributed with this work for additional information regarding copyright
 * ownership.  You may not use this file except in compliance with the License.
 *
 * You may obtain a copy of the License at
 *
 *   rtmp://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */
/*
 * Copyright 2023 bilibili
 */

#pragma once

#include "util/retry_mode.hh"

namespace amadeus {
namespace util {

using namespace seastar;

class retry_runner {
 public:
    retry_runner() = default;
    virtual ~retry_runner() = default;

    future<> run();

    void cancel();

 protected:
    future<> do_try();

    virtual future<> try_once(int times, int total_times) = 0;
    virtual void on_retry_finished() = 0;

    bool _finished = false;
    bool _keep_alive = false;

    std::unique_ptr<retry_mode> _retry_mode;

    void init();

 private:
    void finished();

    virtual retry_mode *mode() const = 0;
};

class delay_retry_runner : public retry_runner {
 public:
    delay_retry_runner(std::unique_ptr<delay_retry_mode> mode);
    virtual ~delay_retry_runner() = default;

 protected:
    std::unique_ptr<delay_retry_mode> _retry_mode;

 private:
    virtual retry_mode *mode() const override {
        return _retry_mode.get();
    }
};

class timeout_retry_runner : public retry_runner {
 public:
    timeout_retry_runner(std::unique_ptr<timeout_retry_mode> mode);
    virtual ~timeout_retry_runner() = default;

 protected:
    std::unique_ptr<timeout_retry_mode> _retry_mode;

 private:
    virtual retry_mode *mode() const override {
        return _retry_mode.get();
    }
};

} // namespace util
} // namespace amadeus
