qt_add_library(qtd_client STATIC)
set_source_files_properties(Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)
qt_add_qml_module(qtd_client
    URI QtowerDefense.Client
    VERSION 1.0
    CLASS_NAME QtowerDefenseClientPlugin
    DEPENDENCIES QtQuick3D QtMultimedia
    SOURCES
        app_info.cpp app_info.h
        game_session.cpp game_session_cards.cpp game_session_save.cpp game_session.h
        graphics.cpp graphics.h
        ground_field.cpp ground_field.h
        landscape.cpp landscape.h
        landscape_view.cpp landscape_view.h
        map_instances.cpp map_instances.h
        scenery_instances.cpp scenery_instances.h
        screen_control.cpp screen_control.h
        terrain_mesh.cpp terrain_mesh.h
        terrain_picture.cpp terrain_picture.h
        world_session.cpp world_session.h
    QML_FILES AssetModel.qml CardFlight.qml CardShop.qml CoinLayer.qml DiceRoll.qml FramedPanel.qml GameIcon.qml GameSounds.qml Ground.qml Hand.qml Hero.qml HeroFace.qml HeroVoice.qml Hud.qml HudButton.qml HudFrame.qml
        IconPaths.js Main.qml MainScene.qml Theme.qml TowerCard.qml WorldMap.qml WorldPainting.qml WorldScene.qml)
target_link_libraries(qtd_client PUBLIC qtd_core qtd_automation Qt6::Gui Qt6::Qml Qt6::Quick Qt6::Quick3D Qt6::Multimedia)
# rhi/qrhi.h, only for the GPU time of a frame.
target_link_libraries(qtd_client PRIVATE Qt6::GuiPrivate)
qtd_set_strict_warnings(qtd_client)

# In the program, not next to it: no platform promises a file system for its assets.
qt_add_resources(qtd_client "qtd_content"
    PREFIX "/content"
    BASE "${PROJECT_SOURCE_DIR}/content"
    FILES
        "${PROJECT_SOURCE_DIR}/content/landscapes.json"
        "${PROJECT_SOURCE_DIR}/content/training_ground.json"
        "${PROJECT_SOURCE_DIR}/content/mire_siege.json"
        "${PROJECT_SOURCE_DIR}/content/world.json")

file(GLOB qtd_assets CONFIGURE_DEPENDS "assets/*.mesh" "assets/*.png" "assets/*.ogg")
qt_add_resources(qtd_client "qtd_assets"
    PREFIX "/assets"
    BASE "assets"
    FILES ${qtd_assets})

# Generated code of qmlcachegen is third-party code in the sense of the warning rule: C4702 comes
# from Qt templates that it instantiates, not from anything in this project.
get_target_property(qtd_client_sources qtd_client SOURCES)
list(FILTER qtd_client_sources INCLUDE REGEX "/\\.rcc/qmlcache/.*\\.cpp$")
set_source_files_properties(${qtd_client_sources} PROPERTIES COMPILE_OPTIONS /wd4702)
