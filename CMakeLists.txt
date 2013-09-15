cmake_minimum_required(VERSION 2.6)
project(libwebqq)
find_package(Boost 1.54 REQUIRED COMPONENTS log_setup log system thread regex filesystem)

include_directories(${Boost_INCLUDE_DIRS})
include_directories(../avhttp/include/)
include_directories(..)

include_directories(${PROJECT_SOURCE_DIR}/include)

add_library(webqq STATIC src/error_code.cpp src/webqq.cpp
	src/impl/webqq_face.cpp src/impl/webqq_impl.cpp src/impl/buddy_mgr.cpp src/impl/webqq_loop.cpp
)

target_link_libraries(webqq ${Boost_LIBRARIES} soci_sqlite3)
INCLUDE (CheckLibraryExists)
if(NOT WIN32)
check_library_exists(rt clock_gettime "" HAVE_CLOCK_GETTIME)
if(HAVE_CLOCK_GETTIME)
        target_link_libraries(webqq -lrt)
endif(HAVE_CLOCK_GETTIME)
endif()
