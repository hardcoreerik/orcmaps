# Minimal ESP-IDF smoke-test application proving the `orcmap` component
# actually builds and links against a real ESP-IDF toolchain -- not just
# the host-only tests/host and tests/consumer CMake projects. See
# docs/ARCHITECTURE.md "Build system" and STATUS.md for what this closes.
#
# EXTRA_COMPONENT_DIRS points one level above this repo (../..) so
# ESP-IDF's component discovery finds this repository's own root
# CMakeLists.txt as a component named after its folder ("OrcMaps") --
# see main/CMakeLists.txt's REQUIRES. A real external consumer (OrcSDR
# via the component manager) would instead reference it by the
# idf_component.yml-declared name ("orcmap") once fetched as a managed
# dependency -- this local EXTRA_COMPONENT_DIRS wiring is a build-time
# convenience for this in-repo example only, not the intended consumer
# pattern (see PROJECT_TRUTH.md "Public API and Versioning").
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(orcmap_generic_esp32_example)
