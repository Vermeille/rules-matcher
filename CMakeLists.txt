cmake_minimum_required(VERSION 2.8.11)

project(RULE-MATCHER CXX)

add_executable(rule-matcher main.cpp)
target_link_libraries(rule-matcher LINK_PUBLIC httpi glog gflags microhttpd nlp-common)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -g3")
