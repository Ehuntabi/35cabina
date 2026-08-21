/* confirm_screen.h - Confirmacion a pantalla completa antes de una accion.
 *
 * Por que existe: los botones de guardar y los de iniciar/finalizar viaje
 * disparaban al primer toque. Con la autocaravana en marcha, un roce basta, y
 * un repostaje mal apuntado no se ve hasta que revisas el viaje en casa. Aqui
 * se muestra QUE se va a guardar y se pide un segundo toque.
 *
 * Es un overlay dentro de la pantalla que lo abre (mismo patron que
 * entry_screen.c), no una pantalla de LVGL aparte.
 *
 * Uso:
 *     confirm_screen_init(parent);                    // una vez, al construir
 *     confirm_screen_open("Guardar repostaje?",       // titulo
 *                         resumen,                    // texto con lo tecleado
 *                         color, "Guardar",           // color y verbo del si
 *                         cb, datos);                 // que hacer si dice si
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*confirm_cb_t)(void *user_data);

void confirm_screen_init(lv_obj_t *parent);

/* 'body' puede ser NULL o "" si no hay nada que resumir (p.ej. iniciar viaje).
 * 'title', 'body' y 'ok_text' deben seguir vivos mientras el dialogo este
 * abierto: se pasan literales o buffers estaticos. */
/* 'no_text' es el boton de la izquierda; NULL deja "No, corregir", que es lo
 * que toca al repasar un formulario antes de guardarlo. En otras preguntas
 * decir "corregir" no significa nada -- al preguntar si una parada ha
 * terminado, el "no" quiere decir "sigo aqui" -- asi que se puede cambiar. */
void confirm_screen_open(const char *title, const char *body,
                         uint32_t color, const char *ok_text, const char *no_text,
                         confirm_cb_t cb, void *user_data);

/* Cierra sin confirmar. Lo llama view_registro_reset() cuando el usuario se va
 * de la pagina con un gesto. */
void confirm_screen_close(void);

/* Aviso de una sola salida: mismo cartel, pero SIN el boton de "No, corregir".
 * Para cuando no hay nada que elegir y solo hay que enterarse -- por ejemplo,
 * que una parada no ha podido abrirse porque la P4 no esta y sin ella no hay
 * reloj. El boton dice 'ok_text' y al pulsarlo solo se cierra. */
void confirm_screen_aviso(const char *title, const char *body,
                          uint32_t color, const char *ok_text);

#ifdef __cplusplus
}
#endif
