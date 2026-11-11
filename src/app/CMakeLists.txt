qt_add_executable(QtowerDefense WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(QtowerDefense PRIVATE qtd_client qtd_clientplugin)
qtd_set_strict_warnings(QtowerDefense)
# The program has no QML of its own. Static Qt (browser, iOS) and the Android packaging find the QML
# modules to bring along only by scanning the QML of the client.
set_target_properties(QtowerDefense PROPERTIES
    QT_QML_ROOT_PATH "${PROJECT_SOURCE_DIR}/src/client"
    QT_QML_IMPORT_SCANNER_EXTRA_ROOT_PATHS "${PROJECT_SOURCE_DIR}/src/client")

# The name of the app on Apple and Android. Change it before the first upload to a store: the stores
# keep an app by it (E-46).
set(QTD_APP_ID "io.github.qtowerdefense")

# The icons are drawn by scripts/make_icon.py. The window takes its own from the resources.
if(WIN32)
    target_sources(QtowerDefense PRIVATE qtowerdefense.rc)
elseif(APPLE)
    set_target_properties(QtowerDefense PROPERTIES
        MACOSX_BUNDLE_GUI_IDENTIFIER "${QTD_APP_ID}"
        MACOSX_BUNDLE_BUNDLE_NAME "QtowerDefense"
        MACOSX_BUNDLE_BUNDLE_VERSION "${PROJECT_VERSION}"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "${PROJECT_VERSION}"
        XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "${QTD_APP_ID}")
    if(IOS)
        # Xcode makes every size from the one picture of 1024 pixels in the asset catalog.
        set_source_files_properties(ios/Assets.xcassets PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        target_sources(QtowerDefense PRIVATE ios/Assets.xcassets)
        set_target_properties(QtowerDefense PROPERTIES
            XCODE_ATTRIBUTE_ASSETCATALOG_COMPILER_APPICON_NAME "AppIcon")
    else()
        set(qtd_icon macos/qtowerdefense.icns)
        set_source_files_properties(${qtd_icon} PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
        target_sources(QtowerDefense PRIVATE ${qtd_icon})
        set_target_properties(QtowerDefense PROPERTIES MACOSX_BUNDLE_ICON_FILE qtowerdefense.icns)
    endif()
elseif(EMSCRIPTEN)
    # The saves live in the IndexedDB of the browser (E-47).
    target_sources(QtowerDefense PRIVATE web_storage.cpp web_storage.h)
    target_link_options(QtowerDefense PRIVATE -lidbfs.js)
    # EM_ASM writes JavaScript, and its $0 is no C++ name.
    set_source_files_properties(web_storage.cpp PROPERTIES
        COMPILE_OPTIONS -Wno-dollar-in-identifier-extension)
elseif(ANDROID)
    # The manifest comes from Qt's template, the folder adds only the launcher icons.
    math(EXPR qtd_version_code
         "${PROJECT_VERSION_MAJOR} * 10000 + ${PROJECT_VERSION_MINOR} * 100 + ${PROJECT_VERSION_PATCH}")
    set_target_properties(QtowerDefense PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_PACKAGE_NAME "${QTD_APP_ID}"
        QT_ANDROID_APP_NAME "QtowerDefense"
        QT_ANDROID_APP_ICON "@mipmap/ic_launcher"
        QT_ANDROID_VERSION_NAME "${PROJECT_VERSION}"
        QT_ANDROID_VERSION_CODE ${qtd_version_code})
endif()

# A self-contained package for the release (E-49): `cmake --install` copies the game, Qt and its
# plugins and QML modules beside it, on macOS into the app bundle. Phones and the browser package
# themselves.
if(QTD_DESKTOP)
    include(GNUInstallDirs)
    install(TARGETS QtowerDefense
        BUNDLE DESTINATION .
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    qt_generate_deploy_qml_app_script(
        TARGET QtowerDefense
        OUTPUT_SCRIPT qtd_deploy_script
        NO_UNSUPPORTED_PLATFORM_ERROR)
    install(SCRIPT ${qtd_deploy_script})
    install(FILES "${PROJECT_SOURCE_DIR}/LICENSE" "${PROJECT_SOURCE_DIR}/README.md" DESTINATION .)
endif()
