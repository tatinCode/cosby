cmake_minimum_required(VERSION 3.21)

project(cosby LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 COMPONENTS Widgets Gui Multimedia Qml Test REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

#recursively grabs the header files and cpp files
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS 
    src/*.cpp
    src/ui/*.cpp
    src/core/*.cpp
    src/scripting/*.cpp
)

file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS 
    include/*.h
    include/ui/*.h
    include/core/*.h
    include/scripting/*.h
)

enable_testing()

add_executable(${PROJECT_NAME} ${SRC_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets 
    Qt6::Gui 
    Qt6::Multimedia 
    Qt6::Qml
    Qt6::Test
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    QT_DEPRECATED_WARNINGS
    #    QT_NO_KEYWORDS
)

#tests

#test for scene player
add_executable(test_scene_player
    test/test_scene_player.cpp
)


target_include_directories(test_scene_player PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_scene_player PRIVATE
    Qt6::Test
    Qt6::Gui
    Qt6::Widgets
)

set_target_properties(test_scene_player PROPERTIES
    AUTOMOC ON
)

add_test(NAME test_scene_player COMMAND test_scene_player)
###

#tests for project file
add_executable(test_project_file
    test/test_project_file.cpp
    src/project/project_file.cpp
)

target_include_directories(test_project_file PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_project_file PRIVATE
    Qt6::Test
)

set_target_properties(test_project_file PROPERTIES
    AUTOMOC ON
)

add_test(NAME test_project_file COMMAND test_project_file)
###

#tests for directory copy
add_executable(test_directory_copy
    test/test_directory_copy.cpp
    src/project/directory_copy.cpp
)

target_include_directories(test_directory_copy PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

target_link_libraries(test_directory_copy PRIVATE
    Qt6::Test
)

set_target_properties(test_directory_copy PROPERTIES
    AUTOMOC ON
)

add_test(NAME test_directory_copy COMMAND test_directory_copy)
###



