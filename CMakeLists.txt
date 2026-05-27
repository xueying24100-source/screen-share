cmake_minimum_required(VERSION 3.16)
project(ScreenShare_Capturer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

set(SOURCES
    main.cpp
    mainwindow.cpp
    annotationoverlay.cpp
    annotationwindow.cpp
    wgctestwindow.cpp
    screencapturer.cpp
    wgcwindowcapturebackend.cpp
    sourceenumerator.cpp
    audiocapturer.cpp
    audioplayer.cpp
    sender.cpp
)

set(HEADERS
    mainwindow.h
    annotationoverlay.h
    annotationwindow.h
    wgctestwindow.h
    screencapturer.h
    wgcwindowcapturebackend.h
    sourceenumerator.h
    audiocapturer.h
    audioplayer.h
    sender.h
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE d3d11 dxgi user32 WindowsApp)
endif()
