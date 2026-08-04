# Thin ESP-IDF entry: boot + call into components/flint
idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES flint
)
