qt_add_executable(QtowerDefense WIN32 MACOSX_BUNDLE main.cpp)
target_link_libraries(QtowerDefense PRIVATE qtd_client qtd_clientplugin)
qtd_set_strict_warnings(QtowerDefense)

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
