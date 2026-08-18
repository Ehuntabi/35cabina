/* main.c
 *
 * 35cabina — satelite tactil de la P4 (ver README.md). Bring-up de
 * hardware (pantalla+tactil+LVGL), recepcion UDP del broadcast de la P4 y
 * el carrusel de 3 pantallas (nav.c). Fase 4 (canal de vuelta hacia la P4)
 * sigue fuera de este repo — ver
 * /home/db3/.claude/plans/polished-chasing-brooks.md.
 */
#include <stdio.h>
#include <inttypes.h>
#include <lvgl.h>
#include "display.h"
#include "esp_bsp.h"
#include "lv_port.h"
#include "data_model.h"
#include "ui/nav.h"
#include "net/udp_rx.h"
#include "esp_log.h"
#include "esp_flash.h"
#include "esp_chip_info.h"
#include "esp_system.h"
#include "esp_heap_caps.h"
#include "esp_timer.h"
#include "esp_task_wdt.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "35CABINA";
#define logSection(section) ESP_LOGI(TAG, "\n\n***** %s *****\n", section)
#define LVGL_PORT_ROTATION_DEGREE 90
#define REBOOT_INTERVAL_US (12ULL * 60 * 60 * 1000000) // 12 horas

static void reboot_timer_cb(void *arg) {
    ESP_LOGI(TAG, "Rebooting after 24h uptime (timer)...");
    esp_restart();
}

/* Heartbeat: diagnostico cada 30s (uptime, heap, PSRAM, contador de vida
 * de LVGL). La recuperacion ante cuelgue la hace lvgl_wdog_task. */
static void heartbeat_task(void *arg) {
    (void)arg;
    while (1) {
        ESP_LOGI("HB", "uptime=%llus free_heap=%u min=%u free_psram=%u lvgl=%u",
                 esp_timer_get_time() / 1000000ULL,
                 (unsigned)esp_get_free_heap_size(),
                 (unsigned)esp_get_minimum_free_heap_size(),
                 (unsigned)heap_caps_get_free_size(MALLOC_CAP_SPIRAM),
                 (unsigned)lvgl_port_get_loop_count());
        vTaskDelay(pdMS_TO_TICKS(30000));
    }
}

/* Supervisor anti-cuelgue respaldado por el Task WDT hardware — ver
 * comentario original en victronsolardisplayesp-multi-device_pantalla_3.5,
 * reutilizado tal cual (bring-up de estabilidad, no logica Victron). */
#define LVGL_WDOG_PERIOD_MS 2000
#define LVGL_WDOG_GRACE_MS  60000
static void lvgl_wdog_task(void *arg) {
    (void)arg;
    ESP_ERROR_CHECK(esp_task_wdt_add(NULL));

    uint32_t prev = lvgl_port_get_loop_count();
    uint32_t stalled_ms = 0;
    while (1) {
        vTaskDelay(pdMS_TO_TICKS(LVGL_WDOG_PERIOD_MS));

        uint32_t now = lvgl_port_get_loop_count();
        if (now != prev) {
            prev = now;
            stalled_ms = 0;
        } else {
            stalled_ms += LVGL_WDOG_PERIOD_MS;
        }

        if (stalled_ms < LVGL_WDOG_GRACE_MS) {
            esp_task_wdt_reset();
        } else {
            ESP_LOGE("WDOG", "UI sin avanzar %ums; dejando saltar el Task WDT",
                     (unsigned)stalled_ms);
        }
    }
}

void setup(void);

void app_main(void) {
    setup();
}

void setup(void) {
    logSection("LVGL init start");

    esp_chip_info_t chip_info;
    esp_chip_info(&chip_info);
    ESP_LOGI(TAG, "This is %s chip, %d cores", CONFIG_IDF_TARGET, chip_info.cores);

    uint32_t flash_size;
    if (esp_flash_get_size(NULL, &flash_size) != ESP_OK) {
        ESP_LOGE(TAG, "Get flash size failed");
        return;
    }
    ESP_LOGI(TAG, "%" PRIu32 "MB flash, min free heap: %" PRIu32 ", free PSRAM: %u",
             flash_size / (1024 * 1024),
             esp_get_minimum_free_heap_size(),
             heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

    logSection("Display init");
    bsp_display_cfg_t cfg = {
        .lvgl_port_cfg = ESP_LVGL_PORT_INIT_CONFIG(),
        .buffer_size   = EXAMPLE_LCD_QSPI_H_RES * EXAMPLE_LCD_QSPI_V_RES,
#if LVGL_PORT_ROTATION_DEGREE == 90
        .rotate        = LV_DISP_ROT_90,
#elif LVGL_PORT_ROTATION_DEGREE == 180
        .rotate        = LV_DISP_ROT_180,
#elif LVGL_PORT_ROTATION_DEGREE == 270
        .rotate        = LV_DISP_ROT_270,
#else
        .rotate        = LV_DISP_ROT_NONE,
#endif
    };
    bsp_display_start_with_config(&cfg);
    bsp_display_brightness_set(5);

    if (!lvgl_port_lock(5000)) {
        ESP_LOGE(TAG, "No se pudo tomar lvgl_port_lock al iniciar UI");
        return;
    }

    esp_err_t nvs_err = nvs_flash_init();
    if (nvs_err == ESP_ERR_NVS_NO_FREE_PAGES || nvs_err == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        nvs_err = nvs_flash_init();
    }
    ESP_ERROR_CHECK(nvs_err);

    data_model_init();
    nav_init();
    lvgl_port_unlock();

    udp_rx_start();

    static esp_timer_handle_t reboot_timer;
    const esp_timer_create_args_t reboot_timer_args = {
        .callback = &reboot_timer_cb,
        .arg = NULL,
        .dispatch_method = ESP_TIMER_TASK,
        .name = "24h_reboot"
    };
    esp_timer_create(&reboot_timer_args, &reboot_timer);
    esp_timer_start_periodic(reboot_timer, REBOOT_INTERVAL_US);

    xTaskCreate(heartbeat_task, "hb", 3072, NULL, 1, NULL);
    xTaskCreate(lvgl_wdog_task, "lvgl_wdog", 3072, NULL, 6, NULL);

    logSection("Setup complete");
}
