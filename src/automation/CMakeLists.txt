qt_add_library(qtd_automation STATIC
    build_info.cpp build_info.h
    headless_runner.cpp headless_runner.h)
target_link_libraries(qtd_automation PUBLIC qtd_core)
qtd_set_strict_warnings(qtd_automation)
