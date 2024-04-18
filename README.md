# 项目名称
自研流媒体服务器Amadeus

# 待开发的功能
- 推流协议支持
    - RTMP-flv推流
    - RTSP-mp4推流
    - HLSv3(.ts)/v7(.fmp4)推流（后期需求）
- 拉流协议支持
    - RTMP-flv拉流
    - RTSP-mp4拉流
    - HTTP-flv拉流（后期需求）
    - HLSv3(.ts)/v7(.fmp4)拉流（后期需求）
- 格式互转
    - flv转mp4
    - mp4转flv
    - 切片生产HLSv3(.ts)/v7(.fmp4)（后期需求）
- 编码互转
    - 视频：H264、HEVCs、AV1互转（后期考虑AV1）
    - 音频：AAC LC、Opus、MP3互转（后期考虑MP3）

# 环境需求
- Ubuntu 22.04 LTS (require)

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
安装seastar
```
cd 3rd/seastar
mkdir -p build && cd build
./configure.py --mode=release --prefix=/usr/local
ninja -C build/release install
```
安装nlohmann_json
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