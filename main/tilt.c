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
 * Usamos un segundo bus I2C por hardware (I2C_NUM_1, el ESP32-S3 tiene dos).
 *
 * PINES: IO17(SDA)/IO18(SCL), los de los conectores JST de 4 pines. Se
 * eligieron sobre los IO5/IO6 del conector "Extended IO" de 8 pines (que
 * tambien valdrian, junto con IO7/IO9/IO14/IO15/IO16) por una razon de
 * cableado, no electrica: el conector de 8 pines lleva SOLO señales, asi que
 * el sensor habria que llevarlo a dos conectores a la vez (datos en uno,
 * 3V3/GND en otro). Los JST de 4 pines llevan 2 GPIO + 3V3 + GND, de modo
 * que el ADXL345 cuelga de UN solo cable de 4 hilos.
 *
 * En el S3 el I2C va por matriz GPIO, asi que reasignarlo es solo cambiar
 * los dos defines de abajo. Evitar: 33-37 (PSRAM octal, ver
 * CONFIG_SPIRAM_MODE_OCT), 26-32 (flash), 19/20 (USB), 0/3/45/46
 * (strapping: los pull-ups del I2C alterarian el arranque) y los que ya usa
 * la pantalla/tactil (1, 4, 8, 21, 38, 39, 40, 45, 47, 48).
 *
 * MAPEO DE EJES: VERIFICADO en placa real el 21-ago-2026 con el sensor montado
 * en su orientacion original. "pitch" sale como cabeceo delante-atras y "roll"
 * como balanceo izda-dcha, en el sentido correcto.
 *
 * El 22-ago-2026 el diseño del soporte obliga a montarlo GIRADO, asi que la
 * orientacion pasa a ser un AJUSTE (0/90/180/270) en vez de estar cableada
 * aqui. Se gira el vector medido antes de calcular los angulos, con lo que todo
 * lo de abajo sigue trabajando siempre en los ejes del VEHICULO y no hay que
 * tocar ni el nivel ni lo que se guarda en las paradas.
 *
 * Ajuste y no constante por dos motivos: el soporte puede volver a cambiar, y
 * acertar el SENTIDO a la primera es dificil -- depende de como quede soldado el
 * ADXL345 respecto a la pantalla. Si con 90 la bola se va al lado contrario, se
 * pone 270 desde la propia pantalla y listo.
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
#define TILT_I2C_SDA_GPIO        GPIO_NUM_17
#define TILT_I2C_SCL_GPIO        GPIO_NUM_18
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

/* Grados que hay girado el sensor respecto al vehiculo. Se lee de NVS al
 * arrancar y cuando se cambia el ajuste. */
static uint16_t s_orientacion;

/* Lleva el vector medido a los ejes del VEHICULO.
 *
 * Girando el aparato 90 grados en sentido horario (mirando la pantalla de
 * frente), el eje X del sensor -- el que apuntaba hacia arriba en la pantalla --
 * pasa a apuntar a la derecha del vehiculo, y el eje Y pasa a apuntar hacia
 * abajo. De ahi el intercambio con el cambio de signo. Z no se toca: el giro es
 * sobre el plano de la pantalla y la gravedad sigue entrando igual por ahi. */
static void a_ejes_vehiculo(float *ax, float *ay)
{
    float x = *ax, y = *ay;
    switch (s_orientacion) {
        case 90:  *ax = -y; *ay =  x; break;
        case 180: *ax = -x; *ay = -y; break;
        case 270: *ax =  y; *ay = -x; break;
        default:  break;                    /* 0: como se monto originalmente */
    }
}

static bool compute_angles(float *pitch_deg, float *roll_deg)
{
    float ax, ay, az;
    if (!read_raw_g(&ax, &ay, &az)) return false;
    a_ejes_vehiculo(&ax, &ay);
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
    s_orientacion      = load_tilt_orientacion();

    ESP_LOGI(TAG, "ADXL345 OK. Orientacion %u grados. Calibracion: pitch_off=%.2f roll_off=%.2f",
             (unsigned)s_orientacion, s_pitch_offset_deg, s_roll_offset_deg);
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

/* Cambiar la orientacion INVALIDA la calibracion: los desvios guardados estan
 * medidos en los ejes de antes, y aplicarlos sobre los nuevos meteria un error
 * silencioso -- el nivel seguiria pintando, pero torcido. Se ponen a cero y se
 * avisa de que hay que volver a calibrar. */
void tilt_set_orientacion(uint16_t grados)
{
    if (grados != 0 && grados != 90 && grados != 180 && grados != 270) return;
    if (grados == s_orientacion) return;

    s_orientacion = grados;
    save_tilt_orientacion(grados);

    s_pitch_offset_deg = 0.0f;
    s_roll_offset_deg  = 0.0f;
    save_tilt_calibration(0, 0);
    ESP_LOGW(TAG, "Orientacion a %u grados. Calibracion BORRADA: hay que "
                  "volver a calibrar con la autocaravana nivelada", (unsigned)grados);
}

uint16_t tilt_get_orientacion(void) { return s_orientacion; }
