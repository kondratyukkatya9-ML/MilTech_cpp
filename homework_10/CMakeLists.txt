cmake_minimum_required(VERSION 3.20)
project(homework_10)

add_executable(drone_sim_mt src/main.cpp)
target_compile_features(drone_sim_mt PRIVATE cxx_std_17)
target_include_directories(drone_sim_mt PRIVATE src)
