cmake_minimum_required(VERSION 3.24)
project(keshos_shell LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.11 REQUIRED COMPONENTS Core Gui Qml Quick)
qt_standard_project_setup(REQUIRES 6.11)

qt_add_executable(kesh_shell
    main.cpp
    system_bridge.h
    system_bridge.cpp
)

qt_add_resources(kesh_shell "app_fonts"
    PREFIX "/fonts"
    FILES fonts/OpenSans.ttf
)

qt_add_qml_module(kesh_shell
    URI KeshOS.Shell
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/Theme.qml
        qml/Wallpaper.qml
        qml/DesktopView.qml
        qml/Panel.qml
        qml/KickoffMenu.qml
        qml/WindowFrame.qml
        qml/AboutDialog.qml
        qml/ExplorerDialog.qml
)

target_link_libraries(kesh_shell PRIVATE Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick)
qt_import_plugins(kesh_shell
    INCLUDE Qt6::QLinuxFbIntegrationPlugin
)
