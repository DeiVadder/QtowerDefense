# Layer order: core <- automation <- client <- programs. scripts/check_layers.py enforces it.
add_subdirectory(core)
add_subdirectory(automation)
add_subdirectory(client)
add_subdirectory(app)
add_subdirectory(headless)
