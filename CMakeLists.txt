cmake_minimum_required(VERSION 3.5)

project(netstuff LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(server NetManager.cxx NetManager.h network.cxx network.h common.h config.h server.cxx)
