qtd_add_test(tst_core_version
    SOURCES core/tst_version.cpp
    LIBS qtd_core
    LABELS core)
# The version the test expects comes from CMake, not from the code under test.
target_compile_definitions(tst_core_version PRIVATE
    QTD_EXPECTED_VERSION="${PROJECT_VERSION}")

qtd_add_test(tst_automation_build_info
    SOURCES automation/tst_build_info.cpp
    LIBS qtd_automation
    LABELS automation)

add_test(NAME headless_build_info COMMAND qtd_headless --build-info)
set_tests_properties(headless_build_info PROPERTIES
    LABELS automation
    PASS_REGULAR_EXPRESSION "^\\{\"program\":\"qtd_headless\",\"version\":\"${PROJECT_VERSION}\"\\}")

# Needs the GPU: renders the real main window once, without real-time waits of its own.
qtd_add_test(tst_client_window
    SOURCES client/tst_window.cpp
    LIBS qtd_client qtd_clientplugin
    LABELS client)

qt_add_executable(tst_qml qml/tst_qml.cpp)
set_target_properties(tst_qml PROPERTIES WIN32_EXECUTABLE OFF)
target_compile_definitions(tst_qml PRIVATE
    QUICK_TEST_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/qml")
target_link_libraries(tst_qml PRIVATE Qt6::QuickTest qtd_client qtd_clientplugin)
qtd_set_strict_warnings(tst_qml)
add_test(NAME tst_qml COMMAND tst_qml)
set_tests_properties(tst_qml PROPERTIES LABELS client TIMEOUT 60)

if(QTD_ENABLE_ASAN)
    # Proves that the sanitizer is really active: this program must die with an ASan report.
    qt_add_executable(asan_selfcheck asan/asan_selfcheck.cpp)
    set_target_properties(asan_selfcheck PROPERTIES WIN32_EXECUTABLE OFF)
    add_test(NAME asan_selfcheck COMMAND asan_selfcheck)
    set_tests_properties(asan_selfcheck PROPERTIES
        LABELS asan
        PASS_REGULAR_EXPRESSION "AddressSanitizer: heap-buffer-overflow")
endif()
