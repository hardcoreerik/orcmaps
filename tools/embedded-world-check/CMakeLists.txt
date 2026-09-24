# Host check for a firmware-embedded world pack (see main.cpp).
#   cmake -S tools/embedded-world-check -B build-embedded-world-check
#   cmake --build build-embedded-world-check --config Release
#   build-embedded-world-check/Release/orcmap_embedded_world_check \
#       orcmaps-world-z4-0.2.0.pmtiles
cmake_minimum_required(VERSION 3.16)
project(orcmaps_embedded_world_check LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ORCMAP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(orcmap_embedded_world_check
    main.cpp
    ${ORCMAP_ROOT}/src/core/geo.cpp
    ${ORCMAP_ROOT}/src/core/viewport.cpp
    ${ORCMAP_ROOT}/src/core/location_picker.cpp
    ${ORCMAP_ROOT}/src/tiles/pmtiles_reader.cpp
    ${ORCMAP_ROOT}/src/tiles/mvt_decoder.cpp
    ${ORCMAP_ROOT}/src/tiles/mvt_stream.cpp
    ${ORCMAP_ROOT}/src/tiles/mvt_translate.cpp
    ${ORCMAP_ROOT}/src/tiles/mvt_classify_experimental.cpp
    ${ORCMAP_ROOT}/src/tiles/compression.cpp
    ${ORCMAP_ROOT}/src/render/style.cpp
    ${ORCMAP_ROOT}/src/render/renderer.cpp
    ${ORCMAP_ROOT}/src/render/clip.cpp
    ${ORCMAP_ROOT}/adapters/host/framebuffer_target.cpp
    ${ORCMAP_ROOT}/third_party/miniz/miniz.c
    ${ORCMAP_ROOT}/third_party/miniz/miniz_tinfl.c
)
target_include_directories(orcmap_embedded_world_check PRIVATE
    ${ORCMAP_ROOT}/include
    ${ORCMAP_ROOT}/adapters/host
    ${ORCMAP_ROOT}/third_party/miniz
)
target_compile_definitions(orcmap_embedded_world_check PRIVATE
    MINIZ_NO_DEFLATE_APIS
    MINIZ_NO_ZLIB_APIS
    MINIZ_NO_ARCHIVE_APIS
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)
