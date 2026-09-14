idf_component_register(
    SRCS "main.cpp"
    INCLUDE_DIRS "."
    REQUIRES OrcMaps m5unified m5gfx fatfs sdmmc vfs esp_timer esp_psram
)
