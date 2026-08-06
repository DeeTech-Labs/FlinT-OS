# Project build / flash defaults (not product firmware config).
#
# | File | Role |
# |------|------|
# | `sdkconfig.defaults` | ESP-IDF Kconfig defaults (wired from root CMakeLists.txt) |
# | `partitions.csv` | Custom partition table (4MB dual OTA + storage) |
#
# Product pins/panels: `components/flint/board/`.
# App intervals/secrets: `components/flint/config/`.
