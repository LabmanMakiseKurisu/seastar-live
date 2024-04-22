/*
 * @Author: Amadeus
 * @Date: 2024-04-22 10:51:23
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-22 10:51:30
 * @FilePath: /Amadeus/src/session/log.hh
 * @Description:
 */
#pragma once

#include <seastar/util/log.hh>

namespace amadeus {

namespace session {

extern seastar::logger l;

} // namespace session
} // namespace amadeus