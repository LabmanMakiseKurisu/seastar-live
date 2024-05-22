/*
 * @Author: Amadeus
 * @Date: 2024-05-20 18:49:15
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-20 18:49:25
 * @FilePath: /Amadeus/src/balancer/load_balancer.cc
 * @Description: m
 */
#include "balancer/load_balancer.hh"

namespace amadeus {

load_balancer load_balancer::global = load_balancer();

} // namespace net