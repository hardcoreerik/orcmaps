# OrcMaps -- standalone ESP-IDF component (repo root = component root).
#
# Portable core only (geo math + PMTiles archive reader) so far -- see
# docs/STATUS.md for exact implementation state. adapters/esp_idf sources
# are added here once that ByteSource adapter exists (docs/ROADMAP.md
# Phase 2). Host-side tests do NOT build through this file -- they are a
# separate standalone CMake project at tests/host/CMakeLists.txt, matching
# the pattern already used by hardcoreerik/esp-rtl-sdr, so host tests never
# need an ESP-IDF toolchain.

idf_component_register(
    SRCS "src/core/geo.cpp"
         "src/tiles/pmtiles_reader.cpp"
         "src/render/style.cpp"
         "third_party/miniz/miniz.c"
         "third_party/miniz/miniz_tinfl.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "third_party/miniz"
    REQUIRES ""
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MINIZ_NO_DEFLATE_APIS
    MINIZ_NO_ARCHIVE_APIS
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
