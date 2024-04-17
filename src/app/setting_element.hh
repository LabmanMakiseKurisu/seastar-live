/*
 * @Author: Amadeus
 * @Date: 2024-04-17 17:38:50
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-17 18:28:14
 * @FilePath: /Amadeus/src/app/setting_element.hh
 * @Description: 
 */
#pragma once
#include <nlohmann/json.hpp>
#include <seastar/core/sstring.hh>
#include <seastar/json/json_elements.hh>
#include "seastar/core/sstring.hh"
namespace amadeus {

using namespace seastar;

class Ielement {
 public:
  sstring _name;  // 名称
  virtual ~Ielement() = default;
  virtual void draw() = 0;
};

}  // namespace amadeus