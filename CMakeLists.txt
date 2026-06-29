cmake_minimum_required(VERSION 3.21)

project(lfs_installer VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network Xml)

qt_add_executable(lfs_installer
    src/main.cpp
    src/installerwindow.cpp
    src/installerwindow.h
)

target_link_libraries(lfs_installer PRIVATE Qt6::Widgets Qt6::Network Qt6::Xml)
