# DO NOT USE THIS TO BUILD MINIC
# ONLY FOR IWYU
# To run
#     CC="gcc" CXX="g++" cmake .. 
#     make 2> iwyu.out
#     python3 ../fix_includes.py < iwyu.out
#

cmake_minimum_required(VERSION 3.3) 
project(minic)

file(GLOB_RECURSE Source ../../Source/*.cpp)

find_program(IWYU_PATH NAMES include-what-you-use iwyu)
if(NOT IWYU_PATH)
  message(FATAL_ERROR "Could not find the program include-what-you-use")
endif()
find_program(iwyu_path NAMES include-what-you-use iwyu REQUIRED)

add_executable(minic_cmake ${Source})

target_include_directories(minic_cmake PUBLIC ../../Source)
target_include_directories(minic_cmake PUBLIC ../../Source/nnue)
target_include_directories(minic_cmake PUBLIC ../../Source/nnue/learn)

target_compile_features(minic_cmake PRIVATE cxx_std_20)

set_property(TARGET minic_cmake PROPERTY CXX_INCLUDE_WHAT_YOU_USE ${iwyu_path})

