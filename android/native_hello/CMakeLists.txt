cmake_minimum_required(VERSION 3.20)

project(inferlab_android_native_hello LANGUAGES CXX)

add_executable(inferlab_android_hello native_hello.cpp)
target_compile_features(inferlab_android_hello PRIVATE cxx_std_17)
target_compile_options(inferlab_android_hello PRIVATE -Wall -Wextra -Wpedantic)
