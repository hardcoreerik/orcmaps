# The OrcMaps component is discovered through EXTRA_COMPONENT_DIRS pointing
# at the repository root, so its component name is the checkout folder name.
# Derive it instead of hardcoding "OrcMaps" so this builds from any worktree.
get_filename_component(_orcmap_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
get_filename_component(_orcmap_component "${_orcmap_root}" NAME)

idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES ${_orcmap_component} hardware-benchmark m5unified m5gfx fatfs
             sdmmc vfs esp_timer esp_psram
)

# Measurement-only build option: run the zoom/pan sweep once at boot instead
# of waiting for a touch, so a sweep can be captured over serial without a
# human tapping the screen. Never enable it for a user-facing build -- the
# sweep takes minutes and takes over the display.
#   idf.py -DORCMAP_TAB5_AUTOSWEEP=1 build
if(ORCMAP_TAB5_AUTOSWEEP)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ORCMAP_TAB5_AUTOSWEEP=1)
endif()
