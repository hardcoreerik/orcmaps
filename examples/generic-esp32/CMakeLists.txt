# Minimal ESP-IDF smoke-test application proving a generic ESP-IDF project
# can consume the `orcmap` component with no graphics framework. See
# docs/ARCHITECTURE.md "Build system" and STATUS.md.
#
# This example must NOT depend on M5GFX, M5Unified, or any other display
# library. Graphics-specific demos belong in examples/m5stack-tab5 (or a
# future dedicated integration example).
#
# EXTRA_COMPONENT_DIRS is this repository root (../.. from this file) so
# ESP-IDF discovers the root CMakeLists.txt as a component named after
# its folder ("OrcMaps") -- see main/CMakeLists.txt's REQUIRES. A real
# external consumer (OrcSDR via the component manager) would instead
# reference it by the idf_component.yml-declared name ("orcmap") once
# fetched as a managed dependency -- this local EXTRA_COMPONENT_DIRS
# wiring is a build-time convenience for this in-repo example only, not
# the intended consumer pattern (see PROJECT_TRUTH.md "Public API and
# Versioning").
#
# Configure a target before building, e.g.:
#   idf.py set-target esp32p4
#   idf.py set-target esp32s3
cmake_minimum_required(VERSION 3.16)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../..")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(orcmap_generic_esp32_example)
