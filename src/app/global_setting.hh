#pragma once
#include <seastar/core/app-template.hh>

#include "app/setting_element.hh"

namespace amadeus {

using namespace seastar;
namespace bpo = boost::program_options;
using bpo_map = boost::program_options::variables_map;
using bpo_var = boost::program_options::variable_value;
using bpo_desc = boost::program_options::options_description;

class global_settings {
 protected:
    std::unordered_map<sstring, element_base *> _element_map;
    std::vector<element_base *> _elements;

 public:
    //element<sstring> boost_configuration_file = element<sstring>("cnf", "", "Boost configure file path");
    element<sstring> json_configuration_file = element<sstring>("json", "", "json configure file path");
    element<sstring> rtmp_listen_address = element<sstring>("rtmp-listen", "0.0.0.0:1935", "RTMP listen address");
    element<float> rtmp_min_cache_duration =
        element<float>("rtmp-min-cache-duration", 6.f, "Seconds of cached RTMP key frames");
    element<float> rtmp_max_gop_duration =
        element<float>("rtmp-max-gop-duration", 60.f, "Maximum durtion(s) of RTMP GOP");
    element<float> rtmp_max_gop_bytes =
        element<float>("rtmp-max-gop-bytes", 300 * 1024 * 1024, "Maximum bytes of RTMP GOP");

 public:
    static global_settings global;
    static void setup_app_options(app_template &app);

 public:
    global_settings();
    global_settings(global_settings &&gs);
    global_settings(const global_settings &gs);
    global_settings(const bpo_map &options);

    // 用bpo_map初始化
    void from_boost(const bpo_map &node);

    // 用nlohmann_json初始化
    void from_nlohmann_json(const nlohmann::json &json);
    // 用json_string初始化
    void from_json_string(const sstring &json_string);
    sstring to_json_string() const;

    global_settings &operator=(const global_settings &gs);
    global_settings &operator=(global_settings &&gs);
    // bool operator==(const global_settings & gs) const;
    // bool operator!=(const global_settings & gs) const;

 protected:
    // 设置程序的boost命令行选项
    void _setup_app_options(app_template &app);
    // 设置程序的boost配置文件选项
    void _setup_app_boost_options(app_template &app);

    // 注册单个元素
    inline void Register_element(element_base *e) {
        if (!e) return;
        _elements.push_back(e);
        _element_map[e->_name] = e;
    }

    // 注册所有元素
    void Register();
};
} // namespace amadeus