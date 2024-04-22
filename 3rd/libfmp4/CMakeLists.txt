cmake_minimum_required(VERSION 3.0.0)


add_subdirectory (libflv)
add_subdirectory (libmov)

aux_source_directory(source libfmp4_files)
add_library(fmp4 STATIC ${libfmp4_files})
target_include_directories(fmp4 PUBLIC include libflv/include libmov/include)
target_link_libraries(fmp4 PUBLIC flv_1 mov_1)