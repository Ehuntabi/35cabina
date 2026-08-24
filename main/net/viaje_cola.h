/* viaje_cola.h — apuntes pendientes de entregar a la P4, en NVS.
 * Ver la cabecera del .c: existe porque esta pantalla se apaga con el contacto
 * y los apuntes se hacen justo antes de apagar. */
#pragma once

/* Cuantos apuntes caben esperando. La pantalla de datos lo necesita para avisar
 * ANTES de llegar al tope; el porque de este numero esta en viaje_cola.c. */
#define VIAJE_COLA_CAPACIDAD  16

#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Se llama cada vez que cambia el numero de pendientes, DESDE LA TAREA DE
 * REPARTO (no desde LVGL): si toca widgets, hacerlo con lv_async_call. */
typedef void (*viaje_cola_cambio_cb)(size_t pendientes);

/* Arranca el repartidor. Una vez, al iniciar. */
void viaje_cola_init(viaje_cola_cambio_cb cb);

/* Encola un cuerpo JSON ya montado. false si la cola esta llena o el apunte no
 * cabe: en ese caso hay que AVISAR al usuario, no callarse. */
bool viaje_cola_push(const char *cuerpo);

size_t viaje_cola_pendientes(void);

#ifdef __cplusplus
}
#endif
