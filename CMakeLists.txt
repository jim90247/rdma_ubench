cmake_minimum_required(VERSION 3.10.0)
project(rdma_ubench VERSION 0.1)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)
set(CMAKE_CXX_EXTENSIONS OFF)
#set(CMAKE_BUILD_TYPE RelWithDebInfo)
set(CMAKE_ENABLE_EXPORTS ON)
add_compile_options(-Wall -Wextra -pedantic)

find_package(ZeroMQ REQUIRED)
find_package(gflags REQUIRED)
find_package(spdlog REQUIRED)

add_library(common ib.h ib.cc log.h log.cc zmq_helper.h)
target_link_libraries(common ibverbs libzmq spdlog::spdlog)

add_executable(ubench ubench.cc server.h server.cc client.h client.cc)
target_link_libraries(ubench common gflags_static)
