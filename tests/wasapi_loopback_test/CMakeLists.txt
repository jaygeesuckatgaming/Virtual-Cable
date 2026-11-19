cmake_minimum_required(VERSION 3.20)
project(wasapi_loopback_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(wasapi_loopback_test main.cpp)
target_compile_definitions(wasapi_loopback_test PRIVATE UNICODE _UNICODE)
target_link_libraries(wasapi_loopback_test PRIVATE ole32 uuid winmm)

if (MSVC)
    target_compile_options(wasapi_loopback_test PRIVATE /W3 /permissive-)
    # Static CRT: the tool is copied to clean test machines/VM snapshots that
    # do not have the Visual C++ redistributable installed.
    set_property(TARGET wasapi_loopback_test PROPERTY MSVC_RUNTIME_LIBRARY "MultiThreaded")
endif()
