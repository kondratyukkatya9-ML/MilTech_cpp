cmake_minimum_required(VERSION 3.20)
project(ugv_odometry CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_executable(ugv_odometry src/main.cpp)
target_compile_options(ugv_odometry PRIVATE -Wall -Wextra -pedantic)
