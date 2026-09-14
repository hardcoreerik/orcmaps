# Host-only visual preview: writes a 1280x720 P6 PPM using the same
# ClearMapBackground + RenderFeatureTile path as tests. No M5GFX.
#   cmake -S examples/host-render -B build-host-render -G "Visual Studio 18 2026"
#   cmake --build build-host-render --config Release
#   build-host-render/Release/orcmap_host_render.exe preview.ppm
cmake_minimum_required(VERSION 3.16)
project(orcmap_host_render LANGUAGES CXX C)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(ORCMAP_ROOT ${CMAKE_CURRENT_SOURCE_DIR}/../..)

add_executable(orcmap_host_render
    main.cpp
    ${ORCMAP_ROOT}/src/core/geo.cpp
    ${ORCMAP_ROOT}/src/core/viewport.cpp
    ${ORCMAP_ROOT}/src/render/style.cpp
    ${ORCMAP_ROOT}/src/render/renderer.cpp
    ${ORCMAP_ROOT}/src/render/clip.cpp
    ${ORCMAP_ROOT}/adapters/host/framebuffer_target.cpp
)
target_include_directories(orcmap_host_render PRIVATE
    ${ORCMAP_ROOT}/include
    ${ORCMAP_ROOT}/adapters/host
)
target_compile_definitions(orcmap_host_render PRIVATE
    $<$<CXX_COMPILER_ID:MSVC>:_CRT_SECURE_NO_WARNINGS>
)
