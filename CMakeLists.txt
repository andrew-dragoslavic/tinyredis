cmake_minimum_required(VERSION 3.20)
project(tinyredis LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

#Dependencies: Google Test
find_package(GTest REQUIRED)

#Library
add_library(kvstore src/kvstore.cpp src/repl.cpp)
target_include_directories(kvstore PUBLIC include)

#Tests
enable_testing()
add_executable(kvstore_tests tests/kvstore_tests.cpp)
target_link_libraries(kvstore_tests PRIVATE kvstore GTest::gtest_main)

add_executable(tinyredis src/main.cpp)
target_link_libraries(tinyredis PRIVATE kvstore)

add_executable(tinyredis_server src/server_main.cpp src/server.cpp)
target_link_libraries(tinyredis_server PRIVATE kvstore)

include(GoogleTest)
gtest_discover_tests(kvstore_tests)