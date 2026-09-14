idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "." "${CMAKE_CURRENT_LIST_DIR}/../../../adapters/m5gfx"
    REQUIRES OrcMaps m5gfx
)
