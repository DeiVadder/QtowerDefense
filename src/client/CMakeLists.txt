qt_add_library(qtd_client STATIC)
qt_add_qml_module(qtd_client
    URI QtowerDefense.Client
    VERSION 1.0
    CLASS_NAME QtowerDefenseClientPlugin
    DEPENDENCIES QtQuick3D
    SOURCES
        app_info.cpp app_info.h
        game_session.cpp game_session.h
        graphics.cpp graphics.h
        map_instances.cpp map_instances.h
    QML_FILES Hud.qml HudButton.qml Main.qml MainScene.qml)
target_link_libraries(qtd_client PUBLIC qtd_core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D)
qtd_set_strict_warnings(qtd_client)

# In the program, not next to it: no platform promises a file system for its assets.
qt_add_resources(qtd_client "qtd_content"
    PREFIX "/content"
    BASE "${PROJECT_SOURCE_DIR}/content"
    FILES "${PROJECT_SOURCE_DIR}/content/training_ground.json")

# Generated code of qmlcachegen is third-party code in the sense of the warning rule: C4702 comes
# from Qt templates that it instantiates, not from anything in this project.
get_target_property(qtd_client_sources qtd_client SOURCES)
list(FILTER qtd_client_sources INCLUDE REGEX "/\\.rcc/qmlcache/.*\\.cpp$")
set_source_files_properties(${qtd_client_sources} PROPERTIES COMPILE_OPTIONS /wd4702)
