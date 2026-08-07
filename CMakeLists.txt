cmake_minimum_required(VERSION 3.16)

project(D2MaxLauncher VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Network)

qt_add_executable(D2MaxLauncher WIN32
    src/main.cpp
    src/config/AppConfig.h
    src/config/ConfigStore.cpp
    src/config/ConfigStore.h
    src/util/Log.cpp
    src/util/Log.h
    src/net/ServerClient.cpp
    src/net/ServerClient.h
    src/launch/PeUtils.cpp
    src/launch/PeUtils.h
    src/launch/DllInjector.cpp
    src/launch/DllInjector.h
    src/launch/IniGenerator.cpp
    src/launch/IniGenerator.h
    src/launch/DotaPathDetector.cpp
    src/launch/DotaPathDetector.h
    src/launch/GameLauncher.cpp
    src/launch/GameLauncher.h
    src/ui/LoginDialog.cpp
    src/ui/LoginDialog.h
    src/ui/MainWindow.cpp
    src/ui/MainWindow.h
    resources/resources.qrc
)

if(WIN32)
    # Embeds the application icon into the executable (Explorer, taskbar, alt-tab).
    target_sources(D2MaxLauncher PRIVATE resources/app.rc)
endif()

target_link_libraries(D2MaxLauncher PRIVATE Qt6::Widgets Qt6::Network)

set_target_properties(D2MaxLauncher PROPERTIES
    WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE TRUE
)

install(TARGETS D2MaxLauncher RUNTIME DESTINATION .)
