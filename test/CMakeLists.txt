#everything here builds into <build>/test, away from the app

#one program for all tests; run a single class with e.g. `cosby_tests TestTimelineView`
add_executable(cosby_tests
    test_main.cpp
    cosby_test.h

    test_easing.cpp
    test_scene_player.cpp
    test_project_file.cpp
    test_directory_copy.cpp
    test_project_creator.cpp
    test_beatmap_updater.cpp
    test_timeline.cpp
    test_script_host.cpp
    test_script_editor.cpp
    test_osb_export.cpp
    test_playback.cpp

    #code under test
    ${CMAKE_SOURCE_DIR}/src/project/project_file.cpp
    ${CMAKE_SOURCE_DIR}/src/project/directory_copy.cpp
    ${CMAKE_SOURCE_DIR}/src/project/project_creator.cpp
    ${CMAKE_SOURCE_DIR}/src/project/path_utils.cpp
    ${CMAKE_SOURCE_DIR}/src/project/beatmap_updater.cpp
    ${CMAKE_SOURCE_DIR}/src/project/osu_parser.cpp
    ${CMAKE_SOURCE_DIR}/src/project/asset_paths.cpp
    ${CMAKE_SOURCE_DIR}/src/project/storyboard_export.cpp
    ${CMAKE_SOURCE_DIR}/src/project/asset_check.cpp
    ${CMAKE_SOURCE_DIR}/src/core/easing.cpp
    ${CMAKE_SOURCE_DIR}/src/core/osb_writer.cpp
    ${CMAKE_SOURCE_DIR}/src/core/time_format.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/timeline_model.cpp
    ${CMAKE_SOURCE_DIR}/src/timeline_view.cpp
    ${CMAKE_SOURCE_DIR}/src/scripting/script_host.cpp
    ${CMAKE_SOURCE_DIR}/src/scripting/js_bridge.cpp
    ${CMAKE_SOURCE_DIR}/src/scripting/js_sprite_proxy.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/script_editor.cpp
    ${CMAKE_SOURCE_DIR}/src/ui/plain_script_editor.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/playback_clock.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/sample_schedule.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/sample_mixer.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/sample_bank.cpp
    ${CMAKE_SOURCE_DIR}/src/audio/playback_controller.cpp

    #headers with Q_OBJECT, for AUTOMOC
    ${CMAKE_SOURCE_DIR}/include/timeline_view.h
    ${CMAKE_SOURCE_DIR}/include/scripting/script_host.h
    ${CMAKE_SOURCE_DIR}/include/scripting/js_bridge.h
    ${CMAKE_SOURCE_DIR}/include/scripting/js_sprite_proxy.h
    ${CMAKE_SOURCE_DIR}/include/ui/script_editor.h
    ${CMAKE_SOURCE_DIR}/include/ui/plain_script_editor.h
    ${CMAKE_SOURCE_DIR}/include/audio/sample_mixer.h
    ${CMAKE_SOURCE_DIR}/include/audio/sample_bank.h
    ${CMAKE_SOURCE_DIR}/include/audio/playback_controller.h
)

target_include_directories(cosby_tests PRIVATE
    ${CMAKE_SOURCE_DIR}/include
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_link_libraries(cosby_tests PRIVATE
    Qt6::Test
    Qt6::Widgets
    Qt6::Qml
    Qt6::Multimedia
)

add_test(NAME cosby_tests COMMAND cosby_tests)
set_tests_properties(cosby_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
###

#the monaco editor test stays separate: it starts a real QtWebEngine, which
#has to be set up before the app object exists, and is only built with monaco
if(COSBY_HAS_MONACO)
    add_executable(test_monaco_editor
        test_monaco_editor.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/script_editor.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/monaco/monaco_editor.cpp
        ${CMAKE_SOURCE_DIR}/src/ui/monaco/monaco_bridge.cpp
        ${CMAKE_SOURCE_DIR}/include/ui/script_editor.h
        ${CMAKE_SOURCE_DIR}/include/ui/monaco/monaco_editor.h
        ${CMAKE_SOURCE_DIR}/include/ui/monaco/monaco_bridge.h
        ${CMAKE_SOURCE_DIR}/resources/monaco_host.qrc
    )

    target_include_directories(test_monaco_editor PRIVATE
        ${CMAKE_SOURCE_DIR}/include
    )

    target_link_libraries(test_monaco_editor PRIVATE
        Qt6::Test
        Qt6::Widgets
        Qt6::WebEngineWidgets
        Qt6::WebChannel
    )

    #headless: chromium's GPU thread crashes on the offscreen platform.
    #monaco's files sit next to the app (<build>/monaco), not in <build>/test.
    add_test(NAME test_monaco_editor COMMAND test_monaco_editor)
    set_tests_properties(test_monaco_editor PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QTWEBENGINE_CHROMIUM_FLAGS=--disable-gpu;COSBY_MONACO_DIR=${CMAKE_BINARY_DIR}/monaco"
        TIMEOUT 120
    )
endif()
###
