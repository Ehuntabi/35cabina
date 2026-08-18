/* tilt.c - Driver minimo del ADXL345 (registros directos, sin libreria de
 * terceros -- solo hace falta leer X/Y/Z, no vale la pena una dependencia
 * nueva del component registry para eso).
 *
 * Registros usados (datasheet Analog Devices ADXL345 Rev. G):
 *   0x00 DEVID        -- debe leer 0xE5, sirve de sanity-check del bus
 *   0x2D POWER_CTL     -- bit3 (Measure) = 1 para salir de standby
 *   0x31 DATA_FORMAT   -- bit3 (FULL_RES) = 1, rango +/-2g (bits 0:1 = 00)
 *   0x32..0x37 DATAX0..DATAZ1 -- 6 bytes, little-endian, 13 bit signados
 *
 * En modo FULL_RES la escala es fija: 3.9 mg/LSB, independiente del rango.
 *
 * BUS I2C PROPIO, no el compartido con el tactil (I2C_NUM_0, GPIO4/GPIO8):
 * el esquematico oficial del modulo (JC3248W535EN/5-IO pin distribution,
 * seccion "Extended IO") muestra que GPIO4/GPIO8 son cableado interno
 * pantalla+tactil, SIN pad accesible desde fuera -- no hay donde soldar.
 * El conector de expansion "Extended IO" (JST1.25 8P) si expone
 * IO5/IO6/IO7/IO15/IO16/IO46/IO9/IO14 libres; usamos IO5(SDA)/IO6(SCL) en
 * un segundo bus I2C por hardware (I2C_NUM_1, el ESP32-S3 tiene dos).
 *
 * OJO mapeo de ejes: pitch/roll asumen una orientacion de montaje concreta
 * del sensor respecto al chasis. Falta verificar en la placa real que
 * "pitch" = cabeceo delante-atras y "roll" = balanceo izda-dcha segun como
 * quede fisicamente pegado el ADXL345 -- si sale cruzado, intercambiar
 * ax/ay en las formulas de abajo.
 */
#include "tilt.h"
#include "config_storage.h"
#include "driver/i2c_master.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static const char *TAG = "tilt";

#define TILT_I2C_PORT            I2C_NUM_1
#define TILT_I2C_SDA_GPIO        GPIO_NUM_5
#define TILT_I2C_SCL_GPIO        GPIO_NUM_6
#define TILT_I2C_CLK_HZ          400000

#define ADXL345_ADDR            0x53
#define REG_DEVID                0x00
#define REG_POWER_CTL             0x2D
#define REG_DATA_FORMAT           0x31
#define REG_DATAX0                0x32
#define ADXL345_DEVID_EXPECTED   0xE5

static i2c_master_bus_handle_t s_bus = NULL;
static i2c_master_dev_handle_t s_dev = NULL;
static bool  s_present = false;
static float s_pitch_offset_deg = 0.0f;
static float s_roll_offset_deg  = 0.0f;

static esp_err_t reg_write(uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = {reg, val};
    return i2c_master_transmit(s_dev, buf, sizeof(buf), 100);
}

static esp_err_t reg_read(uint8_t reg, uint8_t *buf, size_t len)
{
    return i2c_master_transmit_receive(s_dev, &reg, 1, buf, len, 100);
}

static bool read_raw_g(float *ax, float *ay, float *az)
{
    if (!s_present) return false;
    uint8_t buf[6];
    if (reg_read(REG_DATAX0, buf, sizeof(buf)) != ESP_OK) return false;

    int16_t x = (int16_t)((buf[1] << 8) | buf[0]);
    int16_t y = (int16_t)((buf[3] << 8) | buf[2]);
    int16_t z = (int16_t)((buf[5] << 8) | buf[4]);
    const float scale = 0.0039f;   /* g/LSB, fijo en modo full-res */
    *ax = x * scale;
    *ay = y * scale;
    *az = z * scale;
    return true;
}

static bool compute_angles(float *pitch_deg, float *roll_deg)
{
    float ax, ay, az;
    if (!read_raw_g(&ax, &ay, &az)) return false;
    *pitch_deg = atan2f(-ax, sqrtf(ay * ay + az * az)) * 180.0f / (float)M_PI;
    *roll_deg  = atan2f(ay, az) * 180.0f / (float)M_PI;
    return true;
}

esp_err_t tilt_init(void)
{
    i2c_master_bus_config_t bus_cfg = {
        .i2c_port = TILT_I2C_PORT,
        .sda_io_num = TILT_I2C_SDA_GPIO,
        .scl_io_num = TILT_I2C_SCL_GPIO,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags.enable_internal_pullup = true,  /* red de seguridad si el cableado no trae pullups */
    };
    esp_err_t err = i2c_new_master_bus(&bus_cfg, &s_bus);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "i2c_new_master_bus (IO%d/IO%d) fallo: %s",
                 TILT_I2C_SDA_GPIO, TILT_I2C_SCL_GPIO, esp_err_to_name(err));
        return err;
    }

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = ADXL345_ADDR,
        .scl_speed_hz    = TILT_I2C_CLK_HZ,
    };
    err = i2c_master_bus_add_device(s_bus, &dev_cfg, &s_dev);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "i2c_master_bus_add_device fallo: %s", esp_err_to_name(err));
        return err;
    }

    uint8_t devid = 0;
    err = reg_read(REG_DEVID, &devid, 1);
    if (err != ESP_OK || devid != ADXL345_DEVID_EXPECTED) {
        ESP_LOGW(TAG, "ADXL345 no responde (err=%s devid=0x%02X, esperado 0x%02X) -- "
                      "sensor no instalado/cableado todavia",
                 esp_err_to_name(err), devid, ADXL345_DEVID_EXPECTED);
        s_present = false;
        return ESP_ERR_NOT_FOUND;
    }

    reg_write(REG_DATA_FORMAT, 0x08);  /* FULL_RES=1, +/-2g */
    reg_write(REG_POWER_CTL, 0x08);    /* Measure=1, sale de standby */
    s_present = true;

    int16_t p_centi = 0, r_centi = 0;
    load_tilt_calibration(&p_centi, &r_centi);
    s_pitch_offset_deg = p_centi / 100.0f;
    s_roll_offset_deg  = r_centi / 100.0f;

    ESP_LOGI(TAG, "ADXL345 OK. Calibracion cargada: pitch_off=%.2f roll_off=%.2f",
             s_pitch_offset_deg, s_roll_offset_deg);
    return ESP_OK;
}

bool tilt_is_present(void)
{
    return s_present;
}

bool tilt_get(float *pitch_deg, float *roll_deg)
{
    float p, r;
    if (!compute_angles(&p, &r)) return false;
    *pitch_deg = p - s_pitch_offset_deg;
    *roll_deg  = r - s_roll_offset_deg;
    return true;
}

void tilt_calibrate(void)
{
    if (!s_present) return;

    const int N = 20;
    float sum_pitch = 0.0f, sum_roll = 0.0f;
    int ok = 0;
    for (int i = 0; i < N; i++) {
        float p, r;
        if (compute_angles(&p, &r)) {
            sum_pitch += p;
            sum_roll  += r;
            ok++;
        }
        vTaskDelay(pdMS_TO_TICKS(20));
    }
    if (ok == 0) {
        ESP_LOGW(TAG, "Calibracion: 0 lecturas validas, se mantiene la anterior");
        return;
    }

    s_pitch_offset_deg = sum_pitch / ok;
    s_roll_offset_deg  = sum_roll / ok;
    save_tilt_calibration((int16_t)(s_pitch_offset_deg * 100.0f),
                          (int16_t)(s_roll_offset_deg * 100.0f));
    ESP_LOGI(TAG, "Calibrado (N=%d): pitch_off=%.2f roll_off=%.2f",
             ok, s_pitch_offset_deg, s_roll_offset_deg);
}
