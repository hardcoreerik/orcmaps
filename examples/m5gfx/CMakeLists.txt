# ESP-IDF compile proof: OrcMaps + M5GFX, no M5Unified, no OrcSDR.
# Graphics-independent consumption remains examples/generic-esp32.
#
# EXTRA_COMPONENT_DIRS is the repository root so ESP-IDF finds OrcMaps.
# Configure a target first, e.g. idf.py set-target esp32p4
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(orcmap_m5gfx_example)
