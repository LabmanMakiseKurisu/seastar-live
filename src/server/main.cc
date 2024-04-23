/*
 * @Author: Amadeus
 * @Date: 2024-04-17 15:59:26
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-04-23 15:55:31
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
#include <seastar/http/api_docs.hh>
#include <seastar/net/inet_address.hh>

#include <fstream>
#include <iostream>

#include "app/global_setting.hh"
#include "server/log.hh"
#include "rtmp/rtmp.hh"
#include "server/rtmp/route_handler.hh"
#include "util/stop_signal.hh"


using namespace seastar;
using namespace amadeus;
using namespace amadeus::server;
namespace bpo = boost::program_options;

void
set_routes(rtmp::routes &r, std::shared_ptr<server::transmition> trans) {
    r.set(rtmp::request::mode::play, new rtmp::route::play_stream_route_handler(trans));
    r.set(rtmp::request::mode::publish, new rtmp::route::publish_stream_route_handler(trans));
}

void
start_rtmp_server(rtmp::server_control &server, std::function<void(rtmp::routes &r)> set_routes) {
    auto listen_address = global_settings::global.rtmp_listen_address();
    auto addr = socket_address(ipv4_addr(listen_address));

    server.start().get();
    server.set_routes(std::move(set_routes)).get();

    listen_options opts;
    opts.reuse_address = true;
    opts.lba = server_socket::load_balancing_algorithm::connection_distribution;

    try {
        server.listen(addr, opts).get();
        l.info("RTMP server listening on {}", addr);
    } catch (...) { l.info("RTMP server failed to listen on {} {}", addr, std::current_exception()); }
}

int
main(int ac, char **av) {
    // 创建seastar app
    app_template app;

    // 添加一个版本项
    app.add_options()("version", bpo::value<bool>()->default_value(false)->implicit_value(true), "show app version");

    // 将global_settings类中默认的配置项添加到app中
    global_settings::setup_app_options(app);

    rtmp::server_control rtmp_server;

    std::shared_ptr<transmition> trans = std::make_shared<transmition>();


    // 不同于run，run_deprecated会处理ac和av，解析命令行
    return app.run_deprecated(ac, av, [&] {
        return async([&] {
            // 拿到命令行解析到的结果，如有版本则显示后退出
            auto &options = app.configuration();
            if (options["version"].as<bool>()) {
                std::cout << "App Version: "
                          << "0.0" << std::endl;
                engine().exit(0);
                return;
            }
            seastar_apps_lib::stop_signal stop_signal;
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
                    } catch (...) { server::l.warn("Failed to load or parse json configuration"); }
                } else {
                    server::l.warn("Unable to open JSON file: {}", json_file_path);
                }
            }

            trans->initialize(global_settings::global);

            start_rtmp_server(rtmp_server, [trans](rtmp::routes &r) {
                set_routes(r, trans);
            });
            engine().at_exit([&] {
                l.info("Stoppping RTMP server");
                return rtmp_server.stop();
            });
            //server::l.info("ready to exit");
            stop_signal.wait().get();
        });
    });
}
