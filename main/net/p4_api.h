/* p4_api.h — envios de la 3.5" a la P4. Ver la cabecera del .c para el porque
 * de HTTP y de que no se pueda llamar desde la tarea de LVGL... salvo que SI:
 * estas funciones vuelven al instante, el trabajo lo hace una tarea aparte y el
 * resultado llega por 'cb', ya en el hilo de LVGL (seguro para tocar widgets).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 'ok' es true solo con respuesta 2xx. 'estado' es el codigo HTTP, o 0 si no se
 * llego a conectar (P4 apagada o fuera de alcance). 401 = credenciales del
 * portal mal puestas en Ajustes. */
typedef void (*p4_api_done_cb)(bool ok, int estado);

/* Devuelven false si ni siquiera se pudo lanzar el envio (sin memoria); en ese
 * caso 'cb' NO se llama. */
bool p4_api_viaje_inicio(uint32_t id, const char *destino, uint32_t fecha_dias,
                         p4_api_done_cb cb);

/* El POST crudo, SINCRONO: bloquea hasta la respuesta. Lo usa el repartidor de
 * la cola (viaje_cola.c), que tiene su propia tarea. NO llamar desde LVGL.
 * 'estado_out' recibe el codigo HTTP, o 0 si no se llego a conectar. */
bool p4_api_post(const char *cuerpo, int *estado_out);

/* Montadores del cuerpo JSON, para encolar sin enviar. */
void p4_api_cuerpo_inicio(char *out, size_t n, uint32_t id,
                          const char *destino, uint32_t fecha_dias);
/* 'eventos' es cuantos apuntes ha generado el viaje, el inicio incluido: con
 * eso la P4 sabe si le falta alguno y marca el viaje como incompleto. */
void p4_api_cuerpo_fin(char *out, size_t n, uint32_t id, uint32_t eventos);

#ifdef __cplusplus
}
#endif
