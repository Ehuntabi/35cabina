/* view_info.h - Pantalla de info agrupada (Fase 1).
 *
 * Grid con las 6 categorias que llegan por mini_msg_t desde la P4:
 * Bateria, Bateria motor, DC/DC, Frigo, Aguas, Exterior. A diferencia del
 * carrusel de una sola card del mini (pantalla de 320x172), aqui hay sitio
 * de sobra para verlas todas a la vez.
 */
#pragma once

#include "lvgl.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Crea el grid dentro de parent (normalmente lv_scr_act()) y arranca el
 * timer de refresco (2 Hz, sondea data_model_get()). */
void view_info_create(lv_obj_t *parent);

/* Pastilla de "N sin enviar". La llama el repartidor de la cola DESDE SU TAREA,
 * asi que por dentro salta a LVGL con lv_async_call. Con 0 se esconde sola.
 *
 * Va en ESTA pantalla y no en un cartel aparte porque es la que esta puesta
 * cuando vas a quitar el contacto, y esta pantalla no puede saber que vas a
 * hacerlo: se queda sin corriente y ya. Por eso el aviso tiene que estar
 * visible todo el rato mientras quede algo, no saltar "al apagar". */
void view_info_set_pendientes(size_t pendientes);

/* Cuantos apuntes quedan ABIERTOS (declarados y sin cerrar). Comparte pastilla
 * con los "sin enviar" -- ver el comentario de pendientes_aplicar().
 *
 * Va aqui y no solo en la pantalla de registros porque esta es la que esta
 * puesta mientras conduces: si el aviso solo vive en la otra, hay que acordarse
 * de ir a mirarlo, que es justo lo que no se hace. */
void view_info_set_sin_cerrar(size_t sin_cerrar);

#ifdef __cplusplus
}
#endif
