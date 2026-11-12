cmake_minimum_required(VERSION 3.20)
project(wasapi_loopback_test CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(wasapi_loopback_test main.cpp)
target_compile_definitions(wasapi_loopback_test PRIVATE UNICODE _UNICODE)
target_link_libraries(wasapi_loopback_test PRIVATE ole32 uuid)

if (MSVC)
    target_compile_options(wasapi_loopback_test PRIVATE /W3 /permissive-)
endif()
