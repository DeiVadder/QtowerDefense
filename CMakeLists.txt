cmake_minimum_required(VERSION 3.30)
project(QtowerDefense VERSION 0.0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(QTD_ENABLE_ASAN "Build everything with MSVC AddressSanitizer" OFF)
option(QTD_ENABLE_UBSAN "Build core and automation with clang-cl UBSan, without the client" OFF)

find_package(Qt6 6.12 REQUIRED COMPONENTS Core Gui GuiPrivate Qml Quick Quick3D Test QuickTest)
qt_standard_project_setup(REQUIRES 6.12)

# qmlls finds the QML modules of the build through this generated file.
set(QT_QML_GENERATE_QMLLS_INI ON)
set(QT_QML_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/qml")

include(cmake/QtdBuild.cmake)
qtd_enable_sanitizers()

enable_testing()
add_subdirectory(src)
add_subdirectory(tests)
