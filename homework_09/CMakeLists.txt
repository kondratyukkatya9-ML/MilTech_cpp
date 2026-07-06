cmake_minimum_required(VERSION 3.20)
project(homework_09)

add_executable(drone_sim src/main.cpp)
target_compile_features(drone_sim PRIVATE cxx_std_17)
target_include_directories(drone_sim PRIVATE src)
