/* brillo.h - Brillo de la pantalla en dos niveles, con memoria.
 *
 * Se alterna con un doble toque en la pantalla de datos (ver view_info.c) y se
 * recuerda al reiniciar (config_storage, namespace "display"). Hasta la v1.10
 * el brillo estaba clavado al 5% en main.c, heredado del ejemplo del
 * fabricante, y no habia forma de cambiarlo.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Los dos unicos valores validos. Si se cambian, el nivel guardado de antes
 * deja de ser valido y brillo_init() cae al ALTO: es a proposito, mas vale
 * pasarse de luz que quedarse con una pantalla que no se ve. */
#define BRILLO_BAJO   30
#define BRILLO_ALTO  100

/* Aplica el nivel guardado (ALTO la primera vez). Necesita NVS ya arrancado y
 * el display ya iniciado, porque el PWM de la retroiluminacion se configura
 * dentro de bsp_display_start_with_config(). */
void brillo_init(void);

/* Cambia al otro nivel, lo aplica y lo guarda. */
void brillo_alternar(void);

/* Nivel actual en %, para quien quiera mostrarlo. */
uint8_t brillo_actual(void);

#ifdef __cplusplus
}
#endif
