file(GLOB SOURCES "*.c" "ui/*.c" "icons/*.c" "net/*.c")

idf_component_register(
    SRCS       ${SOURCES}
    INCLUDE_DIRS "." "ui" "net"
    PRIV_REQUIRES
        config_storage
        lvgl
        esp_lcd
        spi_flash
        nvs_flash
        esp_app_format    # <— version del firmware (esp_app_get_description)
        esp_netif         # <— interfaz TCP/IP para el STA hacia la P4
        esp_event         # <— eventos Wi-Fi/netif
        esp_wifi          # <— asociarse al AP de la P4
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LOG_LOCAL_LEVEL=ESP_LOG_VERBOSE
    LV_LVGL_H_INCLUDE_SIMPLE
)
