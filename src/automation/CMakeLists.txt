qt_add_library(qtd_automation STATIC
    bot.cpp bot.h
    good_bot.cpp good_bot.h
    build_info.cpp build_info.h
    control_session.cpp control_session.h
    control_stdio.cpp control_stdio.h
    headless_runner.cpp headless_runner.h
    invariants.cpp invariants.h
    report.cpp report.h
    statistics.cpp statistics.h)
target_link_libraries(qtd_automation PUBLIC qtd_core)
qtd_set_strict_warnings(qtd_automation)
