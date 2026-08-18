// config_storage.h
#pragma once

#include "esp_err.h"
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include "nvs.h"

// Brightness settings (NVS namespace: "display")
esp_err_t load_brightness(uint8_t *brightness_out);
esp_err_t save_brightness(uint8_t brightness);

// Screensaver settings (NVS namespace: "screensaver")
esp_err_t load_screensaver_settings(bool *enabled, uint8_t *brightness, uint16_t *timeout);
esp_err_t save_screensaver_settings(bool enabled, uint8_t brightness, uint16_t timeout);

// Relay tab configuration persistence (NVS namespace: "relay")
esp_err_t load_relay_config(bool *enabled_out,
                            uint8_t *count_out,
                            uint8_t *pins_out,
                            char (*labels_out)[20],
                            size_t max_pins);

esp_err_t save_relay_config(bool enabled,
                            const uint8_t *pins,
                            const char (*labels)[20],
                            uint8_t count);

// STA Wi-Fi (AP de la P4 a la que se asocia, NVS namespace: "wifi").
// Permite cambiar de P4 (ej. la de repuesto) sin reflashear -- ver
// main/wifi_credentials.h para el valor de fabrica usado la primera vez
// que se arranca con NVS vacia.
// ssid_out/pass_out deben tener espacio para ssid_len/pass_len bytes;
// al volver, *ssid_len/*pass_len quedan con la longitud real (con \0).
esp_err_t load_wifi_config(char *ssid_out, size_t *ssid_len,
                           char *pass_out, size_t *pass_len);
esp_err_t save_wifi_config(const char *ssid, const char *pass);

// Calibracion de nivel del ADXL345 (NVS namespace: "tilt").
// Offsets en centesimas de grado (deg*100), se restan de cada lectura.
// Si no hay calibracion guardada devuelve 0/0 (sin error).
esp_err_t load_tilt_calibration(int16_t *pitch_offset_centi,
                                 int16_t *roll_offset_centi);
esp_err_t save_tilt_calibration(int16_t pitch_offset_centi,
                                 int16_t roll_offset_centi);
