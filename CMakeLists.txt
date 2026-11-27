cmake_minimum_required(VERSION 3.21)

project(cosby LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Qt6 6.5 COMPONENTS Widgets Gui Multimedia Qml Test REQUIRED)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

#monaco script editor (optional; the plain editor is always built as fallback)
option(COSBY_USE_MONACO "Use the Monaco script editor (needs Qt WebEngine and WebChannel)" ON)
set(COSBY_MONACO_ARCHIVE "" CACHE FILEPATH
    "Local monaco-editor npm tarball to use instead of downloading it (offline builds)")

set(COSBY_MONACO_VERSION 0.52.2)
set(COSBY_MONACO_SHA512 18441611999f90e18b75ddd72bcaf2adf5b3dc020ff18ca655788f1c475ec2b52aee6874aab2ab7c72cd09771b07ab139cc2e7399df3b5288a71c885524209c1)
set(COSBY_MONACO_URL https://registry.npmjs.org/monaco-editor/-/monaco-editor-${COSBY_MONACO_VERSION}.tgz)

set(COSBY_HAS_MONACO OFF)

if(COSBY_USE_MONACO)
    find_package(Qt6 6.5 QUIET COMPONENTS WebEngineWidgets WebChannel)

    if(Qt6WebEngineWidgets_FOUND AND Qt6WebChannel_FOUND)
        set(COSBY_HAS_MONACO ON)
    else()
        message(WARNING "Qt WebEngine/WebChannel not found: building with the plain script editor only")
    endif()
endif()

#puts monaco's min/vs folder at <build>/monaco/vs, next to the executable,
#where MonacoEditor looks for it at run time
if(COSBY_HAS_MONACO)
    set(MONACO_DIR ${CMAKE_CURRENT_BINARY_DIR}/monaco)
    set(MONACO_STAMP ${MONACO_DIR}/.version)

    set(installed_monaco_version "")
    if(EXISTS ${MONACO_STAMP})
        file(READ ${MONACO_STAMP} installed_monaco_version)
    endif()

    if(NOT installed_monaco_version STREQUAL COSBY_MONACO_VERSION)
        if(COSBY_MONACO_ARCHIVE)
            set(monaco_archive ${COSBY_MONACO_ARCHIVE})
            set(monaco_download_ok TRUE)
        else()
            set(monaco_archive ${CMAKE_CURRENT_BINARY_DIR}/_deps/monaco-editor-${COSBY_MONACO_VERSION}.tgz)
            message(STATUS "Downloading monaco-editor ${COSBY_MONACO_VERSION}")
            file(DOWNLOAD ${COSBY_MONACO_URL} ${monaco_archive} STATUS monaco_status TLS_VERIFY ON)
            list(GET monaco_status 0 monaco_status_code)
            if(monaco_status_code EQUAL 0)
                set(monaco_download_ok TRUE)
            else()
                set(monaco_download_ok FALSE)
                message(WARNING "monaco-editor download failed (${monaco_status}); "
                    "the app will use the plain editor. Set COSBY_MONACO_ARCHIVE to a local tarball to build offline.")
            endif()
        endif()

        if(monaco_download_ok)
            file(SHA512 ${monaco_archive} monaco_hash)

            if(NOT monaco_hash STREQUAL COSBY_MONACO_SHA512)
                message(WARNING "monaco-editor archive ${monaco_archive} has the wrong checksum; not using it")
                if(NOT COSBY_MONACO_ARCHIVE)
                    file(REMOVE ${monaco_archive})
                endif()
            else()
                set(monaco_extract_dir ${CMAKE_CURRENT_BINARY_DIR}/_deps/monaco-extract)
                file(REMOVE_RECURSE ${monaco_extract_dir} ${MONACO_DIR})
                file(ARCHIVE_EXTRACT INPUT ${monaco_archive} DESTINATION ${monaco_extract_dir} PATTERNS package/min/vs)
                file(MAKE_DIRECTORY ${MONACO_DIR})
                file(RENAME ${monaco_extract_dir}/package/min/vs ${MONACO_DIR}/vs)
                file(REMOVE_RECURSE ${monaco_extract_dir})
                file(WRITE ${MONACO_STAMP} ${COSBY_MONACO_VERSION})
            endif()
        endif()
    endif()
endif()

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

if(NOT COSBY_HAS_MONACO)
    list(FILTER SRC_FILES EXCLUDE REGEX "/src/ui/monaco/")
    list(FILTER HEADER_FILES EXCLUDE REGEX "/include/ui/monaco/")
endif()

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

if(COSBY_HAS_MONACO)
    target_sources(${PROJECT_NAME} PRIVATE resources/monaco_host.qrc)
    target_link_libraries(${PROJECT_NAME} PRIVATE Qt6::WebEngineWidgets Qt6::WebChannel)
    target_compile_definitions(${PROJECT_NAME} PRIVATE COSBY_HAS_MONACO=1)
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE COSBY_HAS_MONACO=0)
endif()

#tests (built into <build>/test)
add_subdirectory(test)
