# Tab5-optimized interactive OrcMaps demo (Layer B) over the reusable
# hardware benchmark harness (Layer A, examples/hardware-benchmark).
#
# EXTRA_COMPONENT_DIRS carries the repository root (the OrcMaps component)
# and the harness. ESP-IDF names a component after its directory, so the
# OrcMaps component name depends on the checkout folder; main/CMakeLists.txt
# derives it rather than hardcoding "OrcMaps", which lets this example build
# from a worktree with any name.
#
# This example pins ESP-IDF 5.5.x + M5Unified 0.2.20 to match the
# hardware-proven OrcSDR Tab5 bring-up. Configure:
#   idf.py set-target esp32p4
cmake_minimum_required(VERSION 3.16)
get_filename_component(_orcmap_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS
    "${_orcmap_root}"
    "${_orcmap_root}/examples/hardware-benchmark")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(orcmap_m5stack_tab5)
