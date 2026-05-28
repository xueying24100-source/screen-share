cmake_minimum_required(VERSION 3.16)
project(ScreenShare_Capturer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

option(SCREENSHARE_BUILD_DEBUG_WINDOWS "Build standalone debug windows" OFF)
option(SCREENSHARE_BUILD_TESTS "Build unit tests" ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Multimedia)

enable_testing()

set(SCREENSHARE_CORE_SOURCES
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
)

set(SCREENSHARE_CORE_HEADERS
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
)

set(SCREENSHARE_DEBUG_WINDOW_SOURCES
    src/ui/main/mainwindow.cpp
    src/platform/windows/debug/wgctestwindow.cpp
)

set(SCREENSHARE_DEBUG_WINDOW_HEADERS
    src/ui/main/mainwindow.h
    src/platform/windows/debug/wgctestwindow.h
)

add_library(screen_share_core STATIC
    ${SCREENSHARE_CORE_SOURCES}
    ${SCREENSHARE_CORE_HEADERS}
)

target_include_directories(screen_share_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

target_link_libraries(screen_share_core PUBLIC
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::Multimedia
)

if(WIN32)
    target_compile_definitions(screen_share_core PUBLIC
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _WIN32_WINNT=0x0A00
    )
    target_link_libraries(screen_share_core PUBLIC d3d11 dxgi user32 windowsapp ole32 mmdevapi propsys avrt)
    target_compile_options(screen_share_core PRIVATE
        /await:strict
        /Zc:__cplusplus
    )
endif()

set(SCREENSHARE_APP_SOURCES src/app/main.cpp)
set(SCREENSHARE_APP_HEADERS)

if(SCREENSHARE_BUILD_DEBUG_WINDOWS)
    list(APPEND SCREENSHARE_APP_SOURCES ${SCREENSHARE_DEBUG_WINDOW_SOURCES})
    list(APPEND SCREENSHARE_APP_HEADERS ${SCREENSHARE_DEBUG_WINDOW_HEADERS})
endif()

add_executable(${PROJECT_NAME}
    ${SCREENSHARE_APP_SOURCES}
    ${SCREENSHARE_APP_HEADERS}
)

target_link_libraries(${PROJECT_NAME} PRIVATE screen_share_core)

if(SCREENSHARE_BUILD_TESTS)
    add_subdirectory(tests)
endif()
