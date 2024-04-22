cmake_minimum_required(VERSION 3.0.0)

aux_source_directory(source libmov_files)
add_library(mov_1 STATIC ${libmov_files})
target_include_directories(mov_1 PUBLIC include)
