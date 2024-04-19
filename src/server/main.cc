/*
 * @Author: Amadeus
 * @Date: 2024-04-17 15:59:26
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-19 18:37:55
 * @FilePath: /Amadeus/src/server/main.cc
 * @Description:
 */
#include <seastar/core/app-template.hh>
#include <seastar/core/print.hh>
#include <seastar/core/prometheus.hh>
#include <seastar/core/reactor.hh>
#include <seastar/core/seastar.hh>
#include <seastar/core/sharded.hh>
#include <seastar/core/thread.hh>

#include <iostream>
#include <fstream>

#include "app/global_setting.hh"

using namespace seastar;
using namespace amadeus;
namespace bpo = boost::program_options;

int main(int ac, char **av) {
    //创建seastar app
    app_template app;

    //添加一个版本项
    app.add_options()("version", bpo::value<bool>()->default_value(false)->implicit_value(true), "show app version");

    //将global_settings类中默认的配置项添加到app中
    global_settings::setup_app_options(app);

    //不同于run，run_deprecated会处理ac和av，解析命令行
    return app.run_deprecated(ac, av, [&] {
        return async([&] {

            //拿到命令行解析到的结果，如有版本则显示后退出
            auto &options = app.configuration();
            if (options["version"].as<bool>()) {
                std::cout << "App Version: " << "0.0" << std::endl;
                engine().exit(0);
                return;
            }

            // 用命令行得到的结果复写global
            global_settings::global.from_boost(options);

            // 如果有json的路径，则尝试从中读取配置再次复写global
            auto json_file_path = options["json"].as<sstring>();
            if (!json_file_path.empty()) {
                std::ifstream ifs(json_file_path);
                if (ifs) {
                    try {
                        nlohmann::json j = nlohmann::json::parse(ifs);
                        global_settings::global.from_nlohmann_json(j);
                    } catch (const nlohmann::json::parse_error &e) {
                        std::cerr << "JSON parsing error: " << e.what() << std::endl;
                    } catch (const std::exception &e) {
                        std::cerr << "Failed to load or parse json configuration: " << e.what() << std::endl;
                    }
                } else {
                    std::cerr << "Unable to open JSON file: " << json_file_path << std::endl;
                }
            }
            return;
        });
    });
}
