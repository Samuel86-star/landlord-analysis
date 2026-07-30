cmake_minimum_required(VERSION 3.16)
project(landlord-native LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include_directories(include)

add_executable(landlord_test test/main_test.cpp)
add_executable(landlord_sampler test/sampler_test.cpp)
