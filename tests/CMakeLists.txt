find_package(Qt6 REQUIRED COMPONENTS Test Core Gui Widgets Multimedia)

function(add_screenshare_test target_name source_file)
    add_executable(${target_name} ${source_file})
    target_link_libraries(${target_name} PRIVATE
        screen_share_core
        Qt6::Test
        Qt6::Core
        Qt6::Gui
        Qt6::Widgets
        Qt6::Multimedia
    )
    target_include_directories(${target_name} PRIVATE ${PROJECT_SOURCE_DIR}/src)
    add_test(NAME ${target_name} COMMAND ${target_name})
endfunction()

add_screenshare_test(test_audio_mixer test_audio_mixer.cpp)
add_screenshare_test(test_annotation_overlay test_annotation_overlay.cpp)
add_screenshare_test(test_sender_queue test_sender_queue.cpp)
add_screenshare_test(test_screen_capturer_helpers test_screen_capturer_helpers.cpp)

if(WIN32)
    add_screenshare_test(test_capture_smoke_windows test_capture_smoke_windows.cpp)
    set_tests_properties(test_capture_smoke_windows PROPERTIES LABELS "Manual")
endif()
