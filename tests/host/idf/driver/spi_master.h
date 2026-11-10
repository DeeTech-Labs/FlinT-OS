#pragma once
#include "esp_err.h"
#include <cstddef>
#include <cstdint>
inline constexpr int SPI2_HOST = 2, SPI_DMA_CH_AUTO = 3;
inline constexpr uint32_t SPI_DEVICE_HALFDUPLEX = 1, SPI_DEVICE_3WIRE = 2, SPI_TRANS_USE_TXDATA = 4;
struct spi_bus_config_t {
    int mosi_io_num{}, miso_io_num{}, sclk_io_num{}, quadwp_io_num{}, quadhd_io_num{};
    int max_transfer_sz{};
};
struct spi_device_interface_config_t {
    int clock_speed_hz{}, mode{}, spics_io_num{}, queue_size{};
    uint32_t flags{};
};
struct spi_transaction_t {
    uint32_t flags{};
    size_t length{};
    uint8_t tx_data[4]{};
    const void *tx_buffer{};
};
struct spi_device_t {};
using spi_device_handle_t = spi_device_t *;
esp_err_t spi_bus_initialize(int, const spi_bus_config_t *, int);
esp_err_t spi_bus_add_device(int, const spi_device_interface_config_t *, spi_device_handle_t *);
esp_err_t spi_bus_remove_device(spi_device_handle_t);
esp_err_t spi_bus_free(int);
esp_err_t spi_device_transmit(spi_device_handle_t, spi_transaction_t *);
