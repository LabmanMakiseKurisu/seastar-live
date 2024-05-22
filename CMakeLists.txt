cmake_minimum_required(VERSION 3.5)
project(Amadeus LANGUAGES CXX)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# seastar options
set (Seastar_APPS OFF)
set (Seastar_DOCS OFF)
set (Seastar_DEMOS OFF)
set (Seastar_TESTING OFF)
set (Seastar_PRIVATE_COMPILE_DEFINITIONS
  NO_EXCEPTION_INTERCEPT
)

# 设置构建类型和构建输出目录
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose the type of build." FORCE)
endif()
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/release)

add_subdirectory (${CMAKE_SOURCE_DIR}/3rd)

# 添加可执行文件
file(GLOB_RECURSE SOURCES "src/*.cc") # 收集所有src目录下的.cc文件

# 创建可执行文件目标
add_executable(server ${SOURCES})

# 为目标添加包含头文件目录
target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

# 链接三方库
if(Seastar_FOUND)
  target_link_libraries(server PRIVATE Seastar::seastar)
else()
  target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rd/seastar/include)
  target_link_libraries(server PRIVATE seastar)
endif()

if(nlohmann_json_FOUND)
  target_link_libraries(server PRIVATE nlohmann_json::nlohmann_json)
else()
  target_include_directories(server PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/3rd/nlohmann_json/nlohmann)
  target_link_libraries(server PRIVATE nlohmann_json)
endif()


target_include_directories (server 
  PUBLIC 
    ${LIBFLV_INCLUDE_DIR}
    ${LIBMOV_INCLUDE_DIR}
    ${LIBMPEGTS_INCLUDE_DIR}
    ${LIBRTMP_INCLUDE_DIR}
    ${LIBFMP4_INCLUDES_DIR}

)

target_link_libraries (server 
  PUBLIC 
    flv
    mov
    mpegts
    rtmp
    fmp4
)