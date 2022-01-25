cmake_minimum_required(VERSION 3.10)

project(rpcserver)

set(CMAKE_CXX_STANDARD 14)

include_directories(../../include)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable(server main_server.cpp)

target_link_libraries(server zmq)