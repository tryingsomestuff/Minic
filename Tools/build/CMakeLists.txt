# DO NOT USE THIS TO BUILD MINIC
# ONLY FOR IWYU
# To run
#     CC="gcc" CXX="g++" cmake -DCMAKE_CXX_INCLUDE_WHAT_YOU_USE=include-what-you-use .. 
#     make 2> iwyu.out
#     fix_includes < iwyu.out
#

cmake_minimum_required(VERSION 3.0) 
project(minic)

file(GLOB_RECURSE Source ../../Source/*.cpp)

add_executable(minic_cmake ${Source})

target_include_directories(minic_cmake PUBLIC ../../Source)
target_include_directories(minic_cmake PUBLIC ../../Source/nnue)
target_include_directories(minic_cmake PUBLIC ../../Source/nnue/learn)

target_compile_features(minic_cmake PRIVATE cxx_std_20)

