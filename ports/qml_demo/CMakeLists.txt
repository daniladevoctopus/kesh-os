cmake_minimum_required(VERSION 3.24)
project(keshos_qml_demo LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.11)

qt_add_executable(qml_demo main.cpp)
qt_add_resources(qml_demo "app_fonts"
    PREFIX "/fonts"
    FILES fonts/OpenSans.ttf
)
qt_add_qml_module(qml_demo
    URI KeshOS.Demo
    VERSION 1.0
    QML_FILES Main.qml
)
target_link_libraries(qml_demo PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
qt_import_plugins(qml_demo
    INCLUDE Qt6::QLinuxFbIntegrationPlugin
)
