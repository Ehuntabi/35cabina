/* view_info.h - Pantalla de info agrupada (Fase 1).
 *
 * Grid con las 6 categorias que llegan por mini_msg_t desde la P4:
 * Bateria, Bateria motor, DC/DC, Frigo, Aguas, Exterior. A diferencia del
 * carrusel de una sola card del mini (pantalla de 320x172), aqui hay sitio
 * de sobra para verlas todas a la vez.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Crea el grid dentro de parent (normalmente lv_scr_act()) y arranca el
 * timer de refresco (2 Hz, sondea data_model_get()). */
void view_info_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
