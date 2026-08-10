cmake_minimum_required(VERSION 3.10)
project(my_super_project LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(ZLIB REQUIRED zlib)
pkg_check_modules(LIBUV REQUIRED libuv)

find_path(LLHTTP_INCLUDE_DIR llhttp.h PATHS $ENV{PREFIX}/include)
find_library(LLHTTP_LIBRARY llhttp PATHS $ENV{PREFIX}/lib)

if(NOT LLHTTP_INCLUDE_DIR OR NOT LLHTTP_LIBRARY)
    message(FATAL_ERROR "llhttp not found in $PREFIX")
endif()

include_directories(
    ${ZLIB_INCLUDE_DIRS}
    ${LIBUV_INCLUDE_DIRS}
    ${LLHTTP_INCLUDE_DIR}
)

link_directories(
    ${ZLIB_LIBRARY_DIRS}
    ${LIBUV_LIBRARY_DIRS}
)

add_executable(my_super_project src/main.cpp)

target_link_libraries(my_super_project
    ${ZLIB_LIBRARIES}
    ${LIBUV_LIBRARIES}
    ${LLHTTP_LIBRARY}
)
