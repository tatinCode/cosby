cmake_minimum_required(VERSION 3.21)

project(cosby LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 COMPONENTS Widgets Gui Multimedia Qml REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

#recursively grabs the header files and cpp files
file(GLOB_RECURSE SRC_FILES CONFIGURE_DEPENDS src/*.cpp)
file(GLOB_RECURSE HEADER_FILES CONFIGURE_DEPENDS include/*.h)

add_executable(${PROJECT_NAME} ${SRC_FILES} ${HEADER_FILES})

target_include_directories(${PROJECT_NAME} PRIVATE ${CMAKE_SOURCE_DIR}/include)

target_link_libraries(${PROJECT_NAME} PRIVATE
    Qt6::Widgets Qt6::Gui Qt6::Multimedia Qt6::Qml
)

target_compile_definitions(${PROJECT_NAME} PRIVATE
    QT_DEPRECATED_WARNINGS
    #    QT_NO_KEYWORDS
)

