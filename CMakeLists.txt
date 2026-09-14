# OrcMaps -- standalone ESP-IDF component (repo root = component root).
#
# Portable core only (geo, viewport, PMTiles reader, MVT decoder, feature
# model, style system, renderer). Host framebuffer is adapters/host, not
# here.
# No M5GFX/M5Unified/graphics-framework dependency -- adapters/m5gfx is
# optional integration, not registered here. adapters/esp_idf sources are
# added here once that ByteSource adapter exists (docs/ROADMAP.md Phase 2).
# Host-side tests do NOT build through this file -- they are a separate
# standalone CMake project at tests/host/CMakeLists.txt, matching the
# pattern already used by hardcoreerik/esp-rtl-sdr, so host tests never
# need an ESP-IDF toolchain. See docs/STATUS.md for implementation state.

idf_component_register(
    SRCS "src/core/geo.cpp"
         "src/core/viewport.cpp"
         "src/tiles/pmtiles_reader.cpp"
         "src/tiles/mvt_decoder.cpp"
         "src/tiles/mvt_translate.cpp"
         "src/tiles/mvt_classify_experimental.cpp"
         "src/render/style.cpp"
         "src/render/renderer.cpp"
         "third_party/miniz/miniz.c"
         "third_party/miniz/miniz_tinfl.c"
    INCLUDE_DIRS "include"
    PRIV_INCLUDE_DIRS "third_party/miniz"
    REQUIRES ""
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    MINIZ_NO_DEFLATE_APIS
    MINIZ_NO_ZLIB_APIS
    MINIZ_NO_ARCHIVE_APIS
)

target_compile_features(${COMPONENT_LIB} PUBLIC cxx_std_17)
