# Project-wide build helpers.

# Strict warnings for first-party code. Qt headers are imported targets and so come in through
# /external:I, which keeps third-party code out of /W4 /WX.
function(qtd_set_strict_warnings target)
    target_compile_options(${target} PRIVATE
        /W4 /WX /permissive- /utf-8 /Zc:__cplusplus /external:W0)
endfunction()

# Qt is not instrumented, so the container annotations of the MSVC STL must stay off: a std::vector
# or std::string that crosses into Qt would report a false container-overflow.
function(qtd_enable_sanitizers)
    if(NOT QTD_ENABLE_ASAN)
        return()
    endif()
    add_compile_options(/fsanitize=address)
    add_compile_definitions(_DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
    add_link_options(/INCREMENTAL:NO)
endfunction()

# qtd_add_test(<name> SOURCES <files> LIBS <targets> LABELS <labels>)
function(qtd_add_test name)
    cmake_parse_arguments(PARSE_ARGV 1 ARG "" "" "SOURCES;LIBS;LABELS")
    qt_add_executable(${name} ${ARG_SOURCES})
    set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE OFF)
    target_link_libraries(${name} PRIVATE Qt6::Test ${ARG_LIBS})
    qtd_set_strict_warnings(${name})
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES LABELS "${ARG_LABELS}" TIMEOUT 60)
endfunction()
