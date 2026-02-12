cmake_minimum_required(VERSION 3.10)
project(ARS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# This tells CMake to use vcpkg packages
find_package(CURL REQUIRED)

add_executable(ARS ARS.cpp)

# Link using vcpkg's target
target_link_libraries(ARS PRIVATE CURL::libcurl)