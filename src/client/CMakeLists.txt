cmake_minimum_required(VERSION 3.10)

project(client)

set(CMAKE_CXX_STANDARD 14)

include_directories(../../include)

set(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/bin)

add_executable(client main_client.cpp)

target_link_libraries(client zmq)