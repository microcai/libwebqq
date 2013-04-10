cmake_minimum_required(VERSION 2.6)
project(libwebqq)
find_package(Boost 1.48 REQUIRED COMPONENTS signals system thread)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(avhttp/include/)
include_directories(..)

add_library(webqq STATIC webqq.cpp webqq_face.cpp url.cpp md5.cpp webqq_impl.cpp)

target_link_libraries(webqq ${Boost_LIBRARIES})
