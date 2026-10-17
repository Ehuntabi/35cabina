file(GLOB SOURCES "*.c" "ui/*.c" "icons/*.c" "fonts/*.c" "net/*.c")

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
        esp_http_client   # <— portal de la P4 (p4_api.c) y OTA (ota_rx.c)
        mbedtls           # <— base64 del volcado y sha256 de la OTA
        app_update        # <— esp_ota_*: actualizacion por Wi-Fi (net/ota_rx.c)
)

target_compile_definitions(${COMPONENT_LIB} PRIVATE
    LOG_LOCAL_LEVEL=ESP_LOG_VERBOSE
    LV_LVGL_H_INCLUDE_SIMPLE
    # MODELO QUE ESTA PANTALLA LE PIDE A LA P4 EN LA ACTUALIZACION (17-oct-2026).
    #
    # net/ota_rx.c es EL MISMO fichero en las dos cabinas y este es el unico dato
    # que cambia: la P4 tiene un .bin por modelo y hay que pedir el de uno mismo.
    #
    # TIENE QUE IR EN LA COMPILACION Y NO SER UN VALOR POR DEFECTO: si la 3,5"
    # pidiera "cabina5", se bajaria el firmware de la pantalla de 5" (otra UI,
    # otras fuentes, otro tamaño) y la dejaria inservible en el siguiente
    # arranque. Sin este define, ota_rx.c no compila a proposito (#error).
    OTA_MODELO="35cabina"
)
