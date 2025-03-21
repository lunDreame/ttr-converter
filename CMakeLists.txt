cmake_minimum_required(VERSION 3.14)
project(TCPToRS485)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Boost 라이브러리 찾기
find_package(Boost REQUIRED COMPONENTS system thread)

add_executable(tcp_to_rs485 tcp_to_rs485.cpp)

# Boost 라이브러리 포함 및 링크
target_include_directories(tcp_to_rs485 PRIVATE ${Boost_INCLUDE_DIRS})
target_link_libraries(tcp_to_rs485 PRIVATE Boost::system Boost::thread)