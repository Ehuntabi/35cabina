/* p4_api.h — envios de la 3.5" a la P4. Ver la cabecera del .c para el porque
 * de HTTP y de que no se pueda llamar desde la tarea de LVGL... salvo que SI:
 * estas funciones vuelven al instante, el trabajo lo hace una tarea aparte y el
 * resultado llega por 'cb', ya en el hilo de LVGL (seguro para tocar widgets).
 */
#pragma once

#include <stdbool.h>
#include <stdint.h>

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
bool p4_api_viaje_fin(uint32_t id, p4_api_done_cb cb);

#ifdef __cplusplus
}
#endif
