/* view_repostaje.h - Formularios de repostaje y cambio de bombona.
 *
 * Fase 2: UI tactil completa (campos + teclado en pantalla). El boton
 * "Guardar" todavia NO envia nada a la P4 -- eso es la Fase 4 (canal
 * mini_cmd_t, fuera de este repo, requiere luz verde aparte).
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

void view_repostaje_create(lv_obj_t *parent);

#ifdef __cplusplus
}
#endif
