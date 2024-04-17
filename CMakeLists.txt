cmake_minimum_required (VERSION 3.5)

# 项目名称和语言
project (Amadeus LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 设置构建类型和构建输出目录
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release)


# seastar options
set (Seastar_APPS OFF)
set (Seastar_DOCS OFF)
set (Seastar_DEMOS OFF)
set (Seastar_TESTING OFF)

set (Seastar_PRIVATE_COMPILE_DEFINITIONS
  NO_EXCEPTION_INTERCEPT
)

#添加子模块
add_subdirectory(${CMAKE_SOURCE_DIR}/3rd/seastar)
#add_subdirectory(${CMAKE_SOURCE_DIR}/3rd/nlohmann_json)

#寻找三方库
#find_package (Seastar REQUIRED)


# 添加可执行文件
file(GLOB_RECURSE SOURCES "src/*.cc") # 收集所有 src 目录下的 .cpp 文件

# 创建可执行文件目标
add_executable(server ${SOURCES})

# 为目标添加包含目录
target_include_directories(server PRIVATE 
  ${CMAKE_SOURCE_DIR}/3rd/seastar/include
  ${CMAKE_SOURCE_DIR}/3rd/nlohmann_json/single_include
)

#连接三方库
target_link_libraries (server PRIVATE Seastar::seastar)
# 链接 nlohmann_json
#target_link_libraries(server PRIVATE nlohmann_json::nlohmann_json)
