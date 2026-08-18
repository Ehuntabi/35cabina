file(GLOB SOURCES "*.c" "ui/*.c" "icons/*.c")

idf_component_register(
    SRCS       ${SOURCES}
    INCLUDE_DIRS "." "ui"
    PRIV_REQUIRES
        config_storage
        lvgl
        esp_lcd
        spi_flash
        nvs_flash
        esp_app_format    # <— version del firmware (esp_app_get_description)
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LOG_LOCAL_LEVEL=ESP_LOG_VERBOSE
    LV_LVGL_H_INCLUDE_SIMPLE
)
