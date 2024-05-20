cmake_minimum_required (VERSION 3.5)

project (third_party
  VERSION 1.0
  LANGUAGES CXX C)


# 尝试寻找本地安装的Seastar
find_package(Seastar QUIET)

if(Seastar_FOUND)
  message(STATUS "Using system-installed Seastar")
else()
  message(STATUS "Seastar not found in system, using submodule")
  add_subdirectory(${CMAKE_SOURCE_DIR}/3rd/seastar)
endif()

# 尝试寻找本地安装的nlohmann_json
find_package(nlohmann_json QUIET)

if(nlohmann_json_FOUND)
  message(STATUS "Using system-installed nlohmann_json")
else()
  message(STATUS "nlohmann_json not found in system, using submodule")
  add_subdirectory(${CMAKE_SOURCE_DIR}/3rd/nlohmann_json)
endif()

# import fmp4
add_subdirectory (libfmp4)
set(LIBFMP4_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/libfmp4)
set(LIBFMP4_SOURCE_DIR ${LIBFMP4_ROOT}/source)
set(LIBFMP4_INCLUDES_DIR ${LIBFMP4_ROOT}/include)

# import flv
set(LIBFLV_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/media-server/libflv)
set(LIBFLV_SOURCE_DIR ${LIBFLV_ROOT}/source)
set(LIBFLV_INCLUDE_DIR ${LIBFLV_ROOT}/include)

file(GLOB LIBFLV_SRCS
  "${LIBFLV_INCLUDE_DIR}/*.h"
  "${LIBFLV_SOURCE_DIR}/*.c"
)

file(GLOB LIBFLV_HDRS
  "${LIBFLV_INCLUDE_DIR}/*.h"
)

add_library(flv SHARED ${LIBFLV_SRCS})
# set_target_properties(flv 
#   PROPERTIES
#    LINKER_LANGUAGE C
#    INTERFACE_INCLUDE_DIRECTORIES ${LIBFLV_INCLUDE_DIR}
# )
set_target_properties(flv 
  PROPERTIES 
    PUBLIC_HEADER "${LIBFLV_HDRS}"
)

target_include_directories(flv 
  PUBLIC 
    ${LIBFLV_INCLUDE_DIR}
    $<BUILD_INTERFACE:${LIBFLV_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${INSTALL_HEADER_INCLUDE_DIR}/flv>
)

# import mov
set(LIBMOV_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/libmov)
set(LIBMOV_SOURCE_DIR ${LIBMOV_ROOT}/source)
set(LIBMOV_INCLUDE_DIR ${LIBMOV_ROOT}/include)

file(GLOB LIBMOV_SRCS
  "${LIBMOV_INCLUDE_DIR}/*.h"
  "${LIBMOV_SOURCE_DIR}/*.c"
)
file(GLOB LIBMOV_HDRS
  "${LIBMOV_INCLUDE_DIR}/*.h"
)

add_library(mov SHARED ${LIBMOV_SRCS})
# set_target_properties(mov 
#   PROPERTIES
#    LINKER_LANGUAGE C
#    INTERFACE_INCLUDE_DIRECTORIES ${LIBMOV_INCLUDE_DIR}
# )
set_target_properties(mov 
  PROPERTIES 
    PUBLIC_HEADER "${LIBMOV_HDRS}"
)

target_include_directories(mov 
  PUBLIC 
    ${LIBMOV_INCLUDE_DIR}
    $<BUILD_INTERFACE:${LIBMOV_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${INSTALL_HEADER_INCLUDE_DIR}/mov>
)

# import mpegts
set(LIBMPEGTS_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/libmpeg)
set(LIBMPEGTS_SOURCE_DIR ${LIBMPEGTS_ROOT}/source)
set(LIBMPEGTS_INCLUDE_DIR ${LIBMPEGTS_ROOT}/include)

file(GLOB LIBMPEGTS_SRCS
  "${LIBMPEGTS_INCLUDE_DIR}/*.h"
  "${LIBMPEGTS_SOURCE_DIR}/*.c"
)
file(GLOB LIBMPEGTS_HDRS
  "${LIBMPEGTS_INCLUDE_DIR}/*.h"
)

add_library(mpegts SHARED ${LIBMPEGTS_SRCS})

set_target_properties(mpegts 
  PROPERTIES 
    PUBLIC_HEADER "${LIBMPEGTS_HDRS}"
)

target_include_directories(mpegts 
  PUBLIC 
    ${LIBMPEGTS_INCLUDE_DIR}
    $<BUILD_INTERFACE:${LIBMPEGTS_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${INSTALL_HEADER_INCLUDE_DIR}/mpegts>
)

# import rtmp
set(LIBRTMP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/media-server/librtmp)
set(LIBRTMP_SOURCE_DIR ${LIBRTMP_ROOT}/source)
set(LIBRTMP_INCLUDE_DIR ${LIBRTMP_ROOT}/include)

file(GLOB LIBRTMP_SRCS
  "${LIBRTMP_INCLUDE_DIR}/*.h"
  "${LIBRTMP_SOURCE_DIR}/*.c"
)

file(GLOB LIBRTMP_HDRS
  "${LIBRTMP_INCLUDE_DIR}/*.h"
)

add_library(rtmp SHARED ${LIBRTMP_SRCS})
# set_target_properties(rtmp 
#   PROPERTIES
#    LINKER_LANGUAGE C
#    INTERFACE_INCLUDE_DIRECTORIES ${LIBRTMP_INCLUDE_DIR}
# )
set_target_properties(rtmp 
  PROPERTIES 
    PUBLIC_HEADER "${LIBRTMP_HDRS}"
)

target_include_directories(rtmp 
  PUBLIC 
    ${LIBRTMP_INCLUDE_DIR}
    $<BUILD_INTERFACE:${LIBRTMP_INCLUDE_DIR}>
    $<INSTALL_INTERFACE:${INSTALL_HEADER_INCLUDE_DIR}/rtmp>
)
target_link_libraries(rtmp 
  PRIVATE 
    flv
)