#用于指定需要的CMake最低版本
cmake_minimum_required(VERSION 3.21.0)
#项目名称
project(BPT VERSION 1.0.0)

# 用于指明构建目标的include目录
include_directories(./include)
add_executable(dump_numbers ./src/dump_numbers.cpp ./src/bpt.cpp)

add_definitions("-Wall -g")


