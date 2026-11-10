#pragma once
#include "esp_err.h"
#include <cstddef>
#include <cstdint>
struct usb_serial_jtag_driver_config_t { uint32_t tx_buffer_size, rx_buffer_size; };
esp_err_t usb_serial_jtag_driver_install(usb_serial_jtag_driver_config_t *);
esp_err_t usb_serial_jtag_driver_uninstall();
bool usb_serial_jtag_is_connected();
int usb_serial_jtag_read_bytes(void *, uint32_t, uint32_t);
int usb_serial_jtag_write_bytes(const void *, size_t, uint32_t);
