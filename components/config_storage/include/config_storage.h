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
