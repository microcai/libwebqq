cmake_minimum_required(VERSION 2.8)
project(urdl)
add_definitions(-DBOOST_ASIO_DISABLE_THREADS)

find_package(Boost 1.46 REQUIRED COMPONENTS system)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(include)
add_definitions(-DURDL_DISABLE_SSL=1)

add_library(urdl STATIC src/urdl.cpp)

target_link_libraries(urdl ${Boost_LIBRARIES})