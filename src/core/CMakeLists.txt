qt_add_library(qtd_core STATIC
    content.cpp content.h
    content_loader.cpp content_loader.h
    fixed.cpp fixed.h
    json_reader.cpp json_reader.h
    random.cpp random.h
    replay.cpp replay.h
    simulation.cpp simulation.h
    state_hash.cpp state_hash.h
    tick_clock.cpp tick_clock.h
    version.cpp version.h)
target_include_directories(qtd_core PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_definitions(qtd_core PRIVATE
    QTD_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    QTD_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    QTD_VERSION_PATCH=${PROJECT_VERSION_PATCH})
target_link_libraries(qtd_core PUBLIC Qt6::Core)
qtd_set_strict_warnings(qtd_core)
