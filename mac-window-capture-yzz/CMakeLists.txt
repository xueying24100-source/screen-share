cmake_minimum_required(VERSION 3.19)
project(mac_window_capture_yzz LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Widgets)

qt_add_executable(mac_window_capture_yzz
    MACOSX_BUNDLE
    main.cpp
    sourceenumerator.cpp
    sourceenumerator.h
    screencapturer.cpp
    screencapturer.h
    sharesourcepicker.cpp
    sharesourcepicker.h
)

target_link_libraries(mac_window_capture_yzz
    PRIVATE
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        "-framework ApplicationServices"
)

