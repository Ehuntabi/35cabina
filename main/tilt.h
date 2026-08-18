/* tilt.h - Acelerometro ADXL345 para el nivel de aparcamiento (Fase 3).
 *
 * Bus I2C PROPIO (I2C_NUM_1, IO5=SDA/IO6=SCL del conector "Extended IO"),
 * NO el compartido con el tactil -- el esquematico oficial del modulo
 * muestra que el bus del tactil (GPIO4/GPIO8) es cableado interno sin pad
 * accesible desde fuera. Funcionalidad totalmente autonoma en esta placa:
 * no depende del P4 ni del GPS.
 *
 * Lectura estatica (autocaravana parada): pitch/roll por arcotangente
 * sobre el vector de gravedad, sin gyro ni fusion de sensores.
 */
#pragma once

#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Crea el bus I2C dedicado (IO5/IO6) y prueba el sensor (arranca medicion
 * si responde). Devuelve ESP_ERR_NOT_FOUND si no hay ADXL345 cableado --
 * no es fatal, tilt_get() simplemente devolvera false y la UI muestra
 * "sensor no encontrado" en vez de crashear. */
esp_err_t tilt_init(void);

bool tilt_is_present(void);

/* Pitch/roll ya calibrados (offset de NVS restado), en grados.
 * Devuelve false si el sensor no esta presente o la lectura I2C fallo. */
bool tilt_get(float *pitch_deg, float *roll_deg);

/* Promedia ~20 lecturas del angulo ACTUAL y las guarda como nuevo "cero"
 * en NVS (config_storage.c, namespace "tilt"). Llamar con la autocaravana
 * parada sobre una referencia de nivel real (nivel de burbuja fisico o
 * superficie conocida). Bloquea ~0.5s (uso desde un boton, no desde LVGL
 * directamente sin feedback). */
void tilt_calibrate(void);

#ifdef __cplusplus
}
#endif
