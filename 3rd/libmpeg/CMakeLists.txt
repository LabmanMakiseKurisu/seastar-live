cmake_minimum_required(VERSION 3.10.0)

aux_source_directory(source mpegts_files)
add_library(mpegts STATIC ${mpegts_files})
target_include_directories(mpegts PUBLIC include)