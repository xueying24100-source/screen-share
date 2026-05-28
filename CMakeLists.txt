cmake_minimum_required(VERSION 3.16)
project(ScreenShare_Capturer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

set(SOURCES
    src/app/main.cpp
    src/ui/main/mainwindow.cpp
    src/ui/main/meetingmainwindow.cpp
    src/ui/picker/sharesourcepicker.cpp
    src/ui/toolbar/sharetoolbar.cpp
    src/ui/preview/localpreviewwindow.cpp
    src/ui/annotation/annotationoverlay.cpp
    src/ui/annotation/annotationwindow.cpp
    src/media/capture/screen/screencapturer.cpp
    src/media/capture/screen/sourceenumerator.cpp
    src/media/capture/audio/audiocapturer.cpp
    src/media/capture/audio/systemaudiocapturer.cpp
    src/media/mixer/audiomixer.cpp
    src/media/playback/audioplayer.cpp
    src/network/sender.cpp
    src/platform/windows/wgc/wgcwindowcapturebackend.cpp
    src/platform/windows/debug/wgctestwindow.cpp
)

set(HEADERS
    src/ui/main/mainwindow.h
    src/ui/main/meetingmainwindow.h
    src/ui/picker/sharesourcepicker.h
    src/ui/toolbar/sharetoolbar.h
    src/ui/preview/localpreviewwindow.h
    src/ui/annotation/annotationoverlay.h
    src/ui/annotation/annotationwindow.h
    src/media/capture/screen/screencapturer.h
    src/media/capture/screen/sourceenumerator.h
    src/media/capture/audio/audiocapturer.h
    src/media/capture/audio/systemaudiocapturer.h
    src/media/mixer/audiomixer.h
    src/media/playback/audioplayer.h
    src/network/sender.h
    src/platform/windows/wgc/wgcwindowcapturebackend.h
    src/platform/windows/debug/wgctestwindow.h
)

add_executable(${PROJECT_NAME}
    ${SOURCES}
    ${HEADERS}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

if(WIN32)
    target_compile_definitions(${PROJECT_NAME} PRIVATE
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
    )
    target_link_libraries(${PROJECT_NAME} PRIVATE d3d11 dxgi user32 windowsapp ole32 mmdevapi propsys avrt)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /await:strict
        /Zc:__cplusplus
    )
endif()
