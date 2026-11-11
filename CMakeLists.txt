cmake_minimum_required(VERSION 3.30)
project(QtowerDefense VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(QTD_ENABLE_ASAN "Build everything with MSVC AddressSanitizer" OFF)
option(QTD_ENABLE_UBSAN "Build core and automation with clang-cl UBSan, without the client" OFF)

# Phones and the browser get the game alone: the tests and qtd_headless need a desktop (E-46, E-47).
if(ANDROID OR IOS OR EMSCRIPTEN)
    set(QTD_DESKTOP OFF)
else()
    set(QTD_DESKTOP ON)
endif()
# Only MSVC is proven warning-free; other compilers warn without failing until their first build.
if(MSVC)
    set(qtd_errors_default ON)
else()
    set(qtd_errors_default OFF)
endif()
option(QTD_WARNINGS_AS_ERRORS "Fail the build on warnings of first-party code" ${qtd_errors_default})

# Qt 6.12 for WebAssembly ships no Qt Multimedia: the browser build is silent (E-47).
if(EMSCRIPTEN)
    set(QTD_SOUND OFF)
else()
    set(QTD_SOUND ON)
endif()

set(qtd_qt_components Core Gui GuiPrivate Qml Quick Quick3D LinguistTools)
if(QTD_SOUND)
    list(APPEND qtd_qt_components Multimedia)
endif()
if(QTD_DESKTOP)
    list(APPEND qtd_qt_components Test QuickTest)
endif()
find_package(Qt6 6.12 REQUIRED COMPONENTS ${qtd_qt_components})
qt_standard_project_setup(REQUIRES 6.12)

# qmlls finds the QML modules of the build through this generated file.
set(QT_QML_GENERATE_QMLLS_INI ON)
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

include(cmake/QtdBuild.cmake)
qtd_enable_sanitizers()

add_subdirectory(src)
if(QTD_DESKTOP)
    enable_testing()
    add_subdirectory(tests)
endif()
