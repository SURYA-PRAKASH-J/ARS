cmake_minimum_required(VERSION 3.10)
project(ARS)
if(MSVC)
    set(variables 
        CMAKE_C_FLAGS_RELEASE CMAKE_C_FLAGS_DEBUG 
        CMAKE_CXX_FLAGS_RELEASE CMAKE_CXX_FLAGS_DEBUG)
    foreach(variable ${variables})
        if(${variable} MATCHES "/MD")
            string(REGEX REPLACE "/MD" "/MT" ${variable} "${${variable}}")
        endif()
    endforeach()
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Force Static Runtime (Removes MSVCP140.dll/VCRUNTIME140.dll)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(VCPKG_TARGET_TRIPLET "x64-windows-static")

set(CURL_USE_STATIC_LIBS ON)
find_package(CURL REQUIRED)

add_executable(ARS ARS.cpp)

target_compile_definitions(ARS PRIVATE CURL_STATICLIB)

target_link_libraries(ARS PRIVATE CURL::libcurl)