# Standalone Tab5 hardware demo: Springfield PMTiles from SD through
# OrcMaps onto the physical 1280x720 display. Not OrcSDR.
#
# EXTRA_COMPONENT_DIRS is the repository root so ESP-IDF finds OrcMaps.
# This example pins ESP-IDF 5.5.x + M5Unified 0.2.20 to match the
# hardware-proven OrcSDR Tab5 bring-up. Configure:
#   idf.py set-target esp32p4
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(orcmap_m5stack_tab5)
