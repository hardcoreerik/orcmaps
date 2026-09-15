# The OrcMaps component is found through EXTRA_COMPONENT_DIRS pointing at the
# repository root, so its component name is the checkout folder name. Derive
# it rather than hardcoding, so this builds from any worktree.
get_filename_component(_orcmap_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
get_filename_component(_orcmap_component "${_orcmap_root}" NAME)

idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES ${_orcmap_component} hardware-benchmark lovyangfx fatfs sdmmc
             esp_driver_spi esp_driver_gpio vfs esp_timer
)

# Measurement-only build option: run the zoom/pan sweep once at boot instead
# of only the fixed scenarios, so a sweep can be captured over serial.
#   idf.py -DORCMAP_CYD_AUTOSWEEP=1 build
if(ORCMAP_CYD_AUTOSWEEP)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ORCMAP_CYD_AUTOSWEEP=1)
endif()

# Runs the fixed cold/warm scenario per installed pack at boot. Measurement
# only: in a plain build the sequence flickers through half-drawn maps,
# because this board renders straight to the panel.
#   idf.py -DORCMAP_CYD_SCENARIOS=1 build
if(ORCMAP_CYD_SCENARIOS)
    target_compile_definitions(${COMPONENT_LIB} PRIVATE ORCMAP_CYD_SCENARIOS=1)
endif()
