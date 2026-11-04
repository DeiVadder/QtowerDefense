# Layer order: core <- automation <- client <- programs. scripts/check_layers.py enforces it.
add_subdirectory(core)
add_subdirectory(automation)
if(NOT QTD_ENABLE_UBSAN)
    add_subdirectory(client)
    add_subdirectory(app)
endif()
add_subdirectory(headless)
