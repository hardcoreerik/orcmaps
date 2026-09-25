# ESP-IDF link gate: builds a tiny app that links OrcMaps' inflate paths, so
# tools/check_miniz_symbols.py can prove on a real ESP32 link that no miniz
# symbol OrcMaps uses resolves to a ROM-absolute address.
#
#   idf.py -C tests/esp_idf_link -B build-esp-idf-link set-target esp32p4 build
#   python tools/check_miniz_symbols.py build-esp-idf-link
#
# Not an example: it is never flashed. The component is found by the repo
# root's folder name, whatever the checkout is called.
cmake_minimum_required(VERSION 3.16)
get_filename_component(_orcmap_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
set(EXTRA_COMPONENT_DIRS "${_orcmap_root}")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(orcmap_esp_idf_link)
