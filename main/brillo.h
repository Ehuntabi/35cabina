/* brillo.h - Brillo de la pantalla en dos niveles.
 *
 * Se alterna con un doble toque (o un toque largo) en la pantalla de datos (ver
 * view_info.c). Hasta la v1.10 el brillo estaba clavado al 5% en main.c,
 * heredado del ejemplo del fabricante, y no habia forma de cambiarlo.
 *
 * YA NO SE APLICA AL ARRANCAR (16-oct-2026): el nivel se sigue guardando
 * (config_storage, namespace "display"), pero al encender la luz va SIEMPRE al
 * maximo. Ver el comentario largo de brillo_init() en brillo.c.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Los dos unicos valores validos. */
#define BRILLO_BAJO   30
#define BRILLO_ALTO  100

/* Enciende la retroiluminacion AL MAXIMO, siempre, mire lo que mire la NVS.
 * Necesita NVS ya arrancado y el display ya iniciado, porque el PWM de la
 * retroiluminacion se configura dentro de bsp_display_start_with_config().
 *
 * NO aplica el nivel guardado a proposito: el 30 % se ve como una pantalla
 * apagada (tema oscuro) y, como el valor se recuerda, un arranque a oscuras se
 * grababa y volvia a pasar. Arrancando al maximo, un desliz bajando el brillo
 * siempre se arregla desenchufando. */
void brillo_init(void);

/* Cambia al otro nivel, lo aplica, lo guarda y devuelve el nivel nuevo.
 *
 * Devuelve el valor porque es util para el log y para quien lo llame, pero OJO:
 * YA NO TIENE QUE MOVER EL CONTRASTE. Estuvo atado a el (100 % = contraste
 * alto) y en la placa se veia como DOS pantallas distintas en vez de la misma
 * mas clara o mas oscura, que es lo que conto el usuario. Mismo arreglo que en
 * la 5" (ver view_info_set_contraste). */
uint8_t brillo_alternar(void);

/* Nivel actual (BRILLO_BAJO o BRILLO_ALTO). Hoy no lo usa nadie para pintar:
 * el contraste ya no sigue al brillo. Se queda como parte de la API. */
uint8_t brillo_nivel(void);

#ifdef __cplusplus
}
#endif
