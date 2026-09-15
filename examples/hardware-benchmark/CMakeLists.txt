# Layer A: reusable, board-agnostic OrcMaps hardware benchmark harness.
#
# REQUIRES only the OrcMaps component. No M5GFX/M5Unified/SDMMC/display
# dependency belongs here -- boards supply a ByteSource, a RenderTarget, a
# clock, and memory probes. Keeping this list short is what makes the same
# harness reusable for the LilyGO T-Display-S3 and CYD ports later.
#
# ESP-IDF names a component after its directory, and OrcMaps is consumed
# via EXTRA_COMPONENT_DIRS pointing at the repository root -- so the name
# depends on the checkout folder ("OrcMaps", a worktree like
# "OrcMaps-global-packs-controls", or anything else a user cloned into).
# Derive it instead of hardcoding, so this example builds from any checkout.
get_filename_component(_orcmap_root "${CMAKE_CURRENT_LIST_DIR}/../.." ABSOLUTE)
get_filename_component(_orcmap_component "${_orcmap_root}" NAME)

idf_component_register(
    SRCS "bench.cpp"
    INCLUDE_DIRS "include"
    REQUIRES ${_orcmap_component}
)
