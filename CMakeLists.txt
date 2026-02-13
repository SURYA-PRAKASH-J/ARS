cmake_minimum_required(VERSION 3.10)
project(ARS)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force Static Runtime (MT instead of MD)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(CURL_USE_STATIC_LIBS ON)
find_package(CURL REQUIRED)

add_executable(ARS ARS.cpp)

target_compile_definitions(ARS PRIVATE CURL_STATICLIB)

target_link_libraries(ARS PRIVATE CURL::libcurl)