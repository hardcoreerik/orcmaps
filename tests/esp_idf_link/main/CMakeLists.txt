get_filename_component(_orcmap_root "${CMAKE_CURRENT_LIST_DIR}/../../.." ABSOLUTE)
get_filename_component(_orcmap_component "${_orcmap_root}" NAME)

idf_component_register(
    SRCS "main.cpp"
    REQUIRES ${_orcmap_component}
)
