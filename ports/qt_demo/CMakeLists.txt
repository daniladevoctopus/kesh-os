cmake_minimum_required(VERSION 3.24)
project(keshos_qt_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_executable(qt_demo main.cpp)
target_link_libraries(qt_demo PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)
qt_import_plugins(qt_demo
    INCLUDE Qt6::QLinuxFbIntegrationPlugin
)
