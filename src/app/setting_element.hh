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

namespace amadeus {

//using namespace seastar;

class Ielement {
public:
    seastar::sstring _name;  // 名称

public:
    virtual void from_seastar_json(seastar::json::json_base_element *e) = 0;
    virtual void to_seastar_json(seastar::json::json_base_element *e) = 0;
    virtual void from_nlohmann_json(const nlohmann::json &j) = 0;
    virtual void to_nlohmann_json(nlohmann::json &j) = 0;
};

}  // namespace amadeus