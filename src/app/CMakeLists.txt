qt_add_executable(QtowerDefense WIN32 main.cpp)
# The icon of the program file on Windows, drawn by scripts/make_icon.py. The window takes its own.
if(WIN32)
    target_sources(QtowerDefense PRIVATE qtowerdefense.rc)
endif()
target_link_libraries(QtowerDefense PRIVATE qtd_client qtd_clientplugin)
qtd_set_strict_warnings(QtowerDefense)
