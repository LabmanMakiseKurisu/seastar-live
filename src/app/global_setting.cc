/*
 * @Author: Amadeus
 * @Date: 2024-04-19 13:40:07
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-23 15:30:54
 * @FilePath: /Amadeus/src/app/global_setting.cc
 * @Description:
 */
#include "app/global_setting.hh"

#include "global_setting.hh"

namespace amadeus {

using namespace seastar;
namespace bpo = boost::program_options;

global_settings global_settings::global = global_settings();

void
global_settings::setup_app_options(app_template &app) {
    // global_settings config;
    global_settings::global._setup_app_options(app);
    global_settings::global._setup_app_boost_options(app);
}

global_settings::global_settings() {
    Register();
}

global_settings::global_settings(global_settings &&gs) {
    Register();
    for (size_t i = 0; i < _elements.size(); i++) {
        auto e = _elements[i];
        auto other = gs._elements[i];

        e->move_from(other);
    }
}

global_settings::global_settings(const global_settings &gs) {
    Register();
    for (size_t i = 0; i < _elements.size(); i++) {
        auto e = _elements[i];
        auto other = gs._elements[i];

        e->copy_from(other);
    }
}

global_settings::global_settings(const bpo_map &options) {
    Register();
    from_boost(options);
}

void
global_settings::Register() {
    Register_element(&json_configuration_file);
    Register_element(&rtmp_listen_address);
    Register_element(&rtmp_min_cache_duration);
    Register_element(&rtmp_max_gop_duration);
    Register_element(&rtmp_max_gop_bytes);
    Register_element(&stream_timeout_interval);
    Register_element(&max_bytes_per_box);
    Register_element(&frame_trace_enabled);
    Register_element(&auto_stop_publish_mode);
}

void
global_settings::_setup_app_options(app_template &app) {
    // 拿到app的desc，之后把所有的元素填进去
    auto &desc = app.get_options_description();
    for (auto e : _elements) desc.add(e->to_boost_option());
}

void
global_settings::_setup_app_boost_options(app_template &app) {
    auto &desc = app.get_conf_file_options_description();
    for (auto e : _elements) desc.add(e->to_boost_option());
}

void
global_settings::from_boost(const bpo_map &node) {
    for (auto e : _elements) e->from_boost(node[e->_name]);
}

void
global_settings::from_nlohmann_json(const nlohmann::json &j) {
    for (auto e : _elements) {
        auto name = e->_name;
        auto it = j.find(name);
        if (it == j.end()) continue;

        e->from_nlohmann_json(*it);
    }
}

void
global_settings::from_json_string(const sstring &json_string) {
    auto j = nlohmann::json::parse(json_string);
    from_nlohmann_json(j);
}

sstring
global_settings::to_json_string() const {
    nlohmann::json j;
    for (auto e : _elements) { e->to_nlohmann_json(j); }
    return j.dump();
}

global_settings &
global_settings::operator=(const global_settings &gs) {
    if (this != &gs) {
        for (size_t i = 0; i < _elements.size(); i++) {
            auto e = _elements[i];
            auto other = gs._elements[i];

            e->copy_from(other);
        }
    }

    return *this;
}

global_settings &
global_settings::operator=(global_settings &&gs) {
    if (this != &gs) {
        for (size_t i = 0; i < _elements.size(); i++) {
            auto e = _elements[i];
            auto other = gs._elements[i];

            e->move_from(other);
        }
    }

    return *this;
}

} // namespace amadeus
