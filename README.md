<!--
 * @Author: Amadeus
 * @Date: 2024-04-19 11:43:06
 * @LastEditors: Amadeus
 * @LastEditTime: 2024-05-22 16:31:28
 * @FilePath: /Amadeus/README.md
 * @Description: 
-->
# 项目名称
自研流媒体服务器Amadeus

# 功能
- 推流协议支持
    - RTMP-flv推流 (done)
    - WebRTC推流(TODO)
    - RTSP推流(TODO)
- 拉流协议支持
    - RTMP-flv拉流 (done)
    - HTTP-flv拉流 (done)
    - HLSv3(.ts)拉流 (done)
    - WebRTC拉流(done)(TODO)
    - RTSP拉流(TODO)
- 格式互转
    - 切片生产HLSv3(.ts) (done)


# 环境需求
- Ubuntu 22.04 LTS (necessary)

# 依赖安装
```
sudo apt update
```
```
sudo ./install-dependencies.sh
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