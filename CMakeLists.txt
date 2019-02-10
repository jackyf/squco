cmake_minimum_required(VERSION 3.12.0)
project(aproj)

set(CMAKE_AUTOMOC ON)
find_package(Qt5Core CONFIG REQUIRED)
find_package(Qt5Test CONFIG REQUIRED)

set(CMAKE_CXX_STANDARD 14)

add_executable(squco-test test.cpp squco.hpp)
target_link_libraries(squco-test Qt5::Core Qt5::Test)

