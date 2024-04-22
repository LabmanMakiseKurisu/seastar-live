/*
 * @Author: Amadeus
 * @Date: 2024-04-22 10:00:26
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-22 10:01:54
 * @FilePath: /Amadeus/src/server/log.hh
 * @Description:
 */
/*
 * @Author: Amadeus
 * @Date: 2024-04-22 10:00:26
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-22 10:01:24
 * @FilePath: /Amadeus/src/server/log.hh
 * @Description:
 */
#pragma once

#include <seastar/core/sstring.hh>
#include <seastar/util/log.hh>

namespace amadeus {
namespace server {
extern seastar::logger l;
} // namespace server
} // namespace amadeus