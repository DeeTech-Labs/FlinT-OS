# FlinT OS — application component (all product logic lives here)

set(FLINT_SRC_DIRS
    anim
    app
    display
    display/fonts
    day_mode
    event
    faces
    i18n
    net
    settings
    shell
    time
    ui
    weather
)

idf_component_register(
    SRC_DIRS ${FLINT_SRC_DIRS}
    INCLUDE_DIRS
        "."
    REQUIRES
        nvs_flash
        esp_event
        esp_netif
        esp_wifi
        esp_http_client
        esp_timer
        freertos
        lwip
        heap
        log
        esp_driver_usb_serial_jtag
        LovyanGFX
)
