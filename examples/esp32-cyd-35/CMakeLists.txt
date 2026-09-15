# OrcMaps benchmark instrument for the "Cheap Yellow Display" 3.5"
# (ESP32-3248S035, ST7796 320x480, ESP32-D0WD-V3, NO PSRAM).
#
# This is a benchmark instrument, not an interactive demo: the board has no
# PSRAM, so the Tab5's offscreen-canvas drag model is impossible here and is
# deliberately not ported. Rendering goes straight to the panel.
#
# Configure:
#   idf.py set-target esp32
cmake_minimum_required(VERSION 3.16)
get_filename_component(_orcmap_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS
    "${_orcmap_root}"
    "${_orcmap_root}/examples/hardware-benchmark")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(orcmap_esp32_cyd_35)
