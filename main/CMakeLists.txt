# Thin ESP-IDF entry: boot + call into src
idf_component_register(
    SRCS "app_main.cpp"
    INCLUDE_DIRS "."
    REQUIRES src
)
