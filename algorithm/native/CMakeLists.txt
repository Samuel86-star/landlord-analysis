cmake_minimum_required(VERSION 3.16)
project(landlord-native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 源码为 UTF-8（含中文注释/字符串）：强制 MSVC 按 UTF-8 解释，避免系统码页（如 936/GBK）
# 导致 C4819 警告与 C2001「常量中有换行符」编译错误。
if(MSVC)
    add_compile_options(/utf-8)
endif()

include_directories(include)

add_executable(landlord_test test/main_test.cpp)
add_executable(landlord_sampler test/sampler_test.cpp)
add_executable(power_split_test extracted/power_split_test.cpp)
add_executable(split_vs_power extracted/verify_split_vs_power.cpp)
