cmake_minimum_required(VERSION 3.16)

project(AirMonitor VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 REQUIRED COMPONENTS Quick Network)

find_package(Qt6 REQUIRED COMPONENTS Quick Network WebSockets)

find_package(Qt6 REQUIRED COMPONENTS Sql)

qt_standard_project_setup(REQUIRES 6.10)

# Content of downloaded design from Figma to Qt or similar
if (EXISTS "${CMAKE_CURRENT_LIST_DIR}/importedcontent/CMakeLists.txt")
    add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/importedcontent)
endif()

qt_add_executable(AirMonitorApp
    main.cpp
)

qt_add_qml_module(AirMonitorApp
    URI AirMonitorApp
    QML_FILES
        Main.qml
        SOURCES src/network/NetworkScanner.h
        SOURCES src/network/NetworkScanner.cpp
        SOURCES src/monitoring/FilterMonitor.h
        SOURCES src/monitoring/FilterMonitor.cpp
        SOURCES src/monitoring/FanController.h
        SOURCES src/monitoring/FanController.cpp
        SOURCES src/notifications/NotificationBuilder.h
        SOURCES src/notifications/NotificationBuilder.cpp
)

set(CMAKE_CXX_STANDARD 17)

set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
set_target_properties(AirMonitorApp PROPERTIES
#    MACOSX_BUNDLE_GUI_IDENTIFIER com.example.AirMonitoApp
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

target_link_libraries(AirMonitorApp
    PRIVATE Qt6::Quick Qt6::Network
    PRIVATE Qt6::Sql
)

install(TARGETS AirMonitorApp
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

target_sources(AirMonitorApp PRIVATE
    src/core/DataTypes.h
    src/network/ConnectionManager.h
    src/network/ConnectionManager.cpp
    src/network/NetworkScanner.h
    src/network/NetworkScanner.cpp
    src/monitoring/ParameterMonitor.h
    src/monitoring/ParameterMonitor.cpp
    src/monitoring/FilterMonitor.h
    src/monitoring/FilterMonitor.cpp
    src/monitoring/FanController.h
    src/monitoring/FanController.cpp
    src/notifications/EventMonitor.h
    src/notifications/EventMonitor.cpp
    src/notifications/NotificationBuilder.h
    src/database/DatabaseManager.h
    src/database/DatabaseManager.cpp
    src/charts/ChartDrawer.cpp
    src/charts/ChartDrawer.h
    src/notifications/NotificationInbox.cpp
    src/notifications/NotificationInbox.h
    src/network/PairingController.cpp
    src/network/PairingController.h
)

target_link_libraries(AirMonitorApp
    PRIVATE Qt6::Quick Qt6::Network Qt6::WebSockets
)

target_include_directories(AirMonitorApp PRIVATE src)