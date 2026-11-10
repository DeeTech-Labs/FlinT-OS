#pragma once
#include "usb_serial_jtag.h"
enum usj_select_notif_t { USJ_SELECT_READ_NOTIF, USJ_SELECT_WRITE_NOTIF, USJ_SELECT_ERROR_NOTIF };
using usj_select_notif_callback_t = void (*)(usj_select_notif_t, int *);
void usb_serial_jtag_set_select_notif_callback(usj_select_notif_callback_t);
bool usb_serial_jtag_read_ready();
