cmake_minimum_required(VERSION 3.10)
project(cosby VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Main executable
add_executable(cosby
    src/main.cpp
    src/storyboard.cpp
    src/sprite.cpp
    src/command.cpp
)

target_include_directories(cosby PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Set output directory
set_target_properties(cosby PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin
)

# Install target
install(TARGETS cosby DESTINATION bin)
