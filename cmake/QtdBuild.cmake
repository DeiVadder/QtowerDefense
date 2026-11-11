# Project-wide build helpers.

# Strict warnings for first-party code. Qt headers are imported targets and so come in through
# /external:I (MSVC) or -isystem (Clang, GCC), which keeps third-party code out of the warnings.
function(qtd_set_strict_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4 /permissive- /utf-8 /Zc:__cplusplus /external:W0
            $<$<BOOL:${QTD_WARNINGS_AS_ERRORS}>:/WX>)
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic $<$<BOOL:${QTD_WARNINGS_AS_ERRORS}>:-Werror>)
    endif()
endfunction()

# Qt is not instrumented, so the container annotations of the MSVC STL must stay off: a std::vector
# or std::string that crosses into Qt would report a false container-overflow.
function(qtd_enable_asan)
    add_compile_options(/fsanitize=address)
    add_compile_definitions(_DISABLE_VECTOR_ANNOTATION _DISABLE_STRING_ANNOTATION)
    add_link_options(/INCREMENTAL:NO)
endfunction()

# Trap mode: a finding executes an illegal instruction, so no test can pass over it. It needs no
# runtime, which matters here: the UBSan runtime of LLVM is static, Qt needs the dynamic CRT (E-22).
# vptr is left out, it does not exist for the MSVC ABI.
function(qtd_enable_ubsan)
    add_compile_options(-fsanitize=undefined -fsanitize-trap=undefined -fno-sanitize=vptr)
endfunction()

function(qtd_enable_sanitizers)
    if(QTD_ENABLE_ASAN)
        qtd_enable_asan()
    endif()
    if(QTD_ENABLE_UBSAN)
        qtd_enable_ubsan()
    endif()
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
