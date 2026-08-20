/* view_registro.h - Menu de registros con iconos (Fase 2, ampliado).
 *
 * Sustituye el tabview inicial de solo repostaje/bombona. Categorias:
 * inicio/fin de viaje, repostaje, peaje, cambio de bombona,
 * mantenimiento. Los botones "Guardar" (y los de viaje) solo loguean por
 * ahora -- el envio real a la P4 es la Fase 4, fuera de este repo.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void view_registro_create(lv_obj_t *parent);

/* Devuelve la vista al menu de iconos, cierre el formulario que estuviera
 * abierto y el editor de campo si lo estaba.
 *
 * Lo llama nav.c al salir de esta pagina del carrusel: si no, al volver te
 * encontrabas el formulario tal y como lo dejaste, y no el menu. Ojo: lo
 * tecleado a medias se pierde, que es lo que se quiere -- te habias ido. */
void view_registro_reset(void);

#ifdef __cplusplus
}
#endif
