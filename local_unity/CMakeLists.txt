cmake_minimum_required(VERSION 3.14)
project(unity C)
add_library(unity STATIC
    src/unity.c
    compat/unity_compat.c
)
target_include_directories(unity PUBLIC src)
target_compile_options(unity PRIVATE
    -include "${CMAKE_SOURCE_DIR}/test/mock/unity_config.h"
)
set_target_properties(unity PROPERTIES POSITION_INDEPENDENT_CODE ON)
