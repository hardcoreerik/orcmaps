# Host-only pack verifier: offline pre-flight for an SD card.
#   cmake -S tools/pack-verify -B build-pack-verify
#   cmake --build build-pack-verify --config Release
#   build-pack-verify/Release/orcmap_pack_verify G:/orcmaps
#
# Uses the same DiscoverPacks() the firmware uses, so there is no second
# implementation that could disagree with the device.
cmake_minimum_required(VERSION 3.16)
project(orcmaps_pack_verify LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ORCMAP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(orcmap_pack_verify
    main.cpp
    ${ORCMAP_ROOT}/src/core/geo.cpp
    ${ORCMAP_ROOT}/src/core/pack.cpp
    ${ORCMAP_ROOT}/src/core/pack_json.cpp
    ${ORCMAP_ROOT}/src/core/pack_discovery.cpp
    ${ORCMAP_ROOT}/src/tiles/pmtiles_reader.cpp
    ${ORCMAP_ROOT}/src/tiles/mvt_stream.cpp
    ${ORCMAP_ROOT}/src/tiles/mvt_decoder.cpp
    ${ORCMAP_ROOT}/src/tiles/compression.cpp
    ${ORCMAP_ROOT}/adapters/host/file_byte_source.cpp
    ${ORCMAP_ROOT}/adapters/host/pack_filesystem.cpp
    ${ORCMAP_ROOT}/third_party/miniz/miniz.c
    ${ORCMAP_ROOT}/third_party/miniz/miniz_tinfl.c
)
target_include_directories(orcmap_pack_verify PRIVATE
    ${ORCMAP_ROOT}/include
    ${ORCMAP_ROOT}/third_party/miniz
    ${ORCMAP_ROOT}/adapters/host
)
target_compile_definitions(orcmap_pack_verify PRIVATE
    MINIZ_NO_DEFLATE_APIS
    MINIZ_NO_ZLIB_APIS
    MINIZ_NO_ARCHIVE_APIS
)
