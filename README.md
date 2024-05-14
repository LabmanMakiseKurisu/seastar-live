<!--
 * @Author: Amadeus
 * @Date: 2024-04-19 11:43:06
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-14 14:16:41
 * @FilePath: /Amadeus/README.md
 * @Description: 
-->
# 项目名称
自研流媒体服务器Amadeus

# 待开发的功能
- 推流协议支持
    - RTMP-flv推流(done)
    - WebRTC推流（后期需求）
    - RTSP推流（后期需求）
- 拉流协议支持
    - RTMP-flv拉流(done)
    - HTTP-flv拉流(done)
    - HLSv3(.ts)/v7(.fmp4)拉流（TODO）
    - WebRTC拉流(done)（后期需求）
    - RTSP-mp4拉流（后期需求）
- 格式互转
    - 切片生产HLSv3(.ts)/v7(.fmp4)（TODO）
- Swagger API
    - 查询信息
    - 转发：本服务作为客户端向其他服务器推流
    - 回源：本服务作为客户端向其他服务器拉流

# 环境需求
- Ubuntu 22.04 LTS (necessary)

# 依赖安装
```
sudo apt update
```
```
sudo ./scripts/install-dependencies.sh
```

# 构建和运行
首先获取全部子模块
```
git submodule update --init --recursive
``` 
安装seastar (not necessary)
```
cd 3rd/seastar
mkdir -p build && cd build
./configure.py --mode=release --prefix=/usr/local
ninja -C build/release install
```
安装nlohmann_json (not necessary)
```
cd 3rd/nlohmann_json
mkdir -p build && cd build
cmake ..
make install
```
最后构建和编译
```
mkdir -p build && cd build
cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja ..
ninja -j4
```

# 文档
https://www.yuque.com/amadeus-kepdi/ceguvv/ylgs07ida3gpknva