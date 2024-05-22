# 使用 Ubuntu 22.04 LTS 作为基础镜像
FROM ubuntu:22.04

# 设置环境变量以避免交互式输入
ENV DEBIAN_FRONTEND=noninteractive

# 更新软件包列表
RUN apt update

# 复制并执行 install-dependencies.sh 脚本
COPY install-dependencies.sh /install-dependencies.sh
RUN chmod +x /install-dependencies.sh && ./install-dependencies.sh

# 设置工作目录
WORKDIR /app

# 复制项目文件到工作目录
COPY . .

# 构建和编译项目
RUN mkdir -p build && cd build && \
    cmake -DCMAKE_BUILD_TYPE=Debug -G Ninja .. && \
    ninja -j4

# 设置容器启动时的默认命令
CMD ["bash"]
