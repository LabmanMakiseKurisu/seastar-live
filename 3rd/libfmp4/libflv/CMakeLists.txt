cmake_minimum_required(VERSION 3.0.0)

aux_source_directory(source libflv_files)
add_library(flv_1 STATIC ${libflv_files})
target_include_directories(flv_1 PUBLIC include)
