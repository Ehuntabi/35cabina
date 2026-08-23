/* p4_api.c — lo que la 3.5" MANDA a la P4 (Fase 4 del diseño, fase 1).
 *
 * Hasta ahora este aparato solo escuchaba (udp_rx.c). Esto es el camino de
 * vuelta: inicio y fin de viaje.
 *
 * Por que HTTP y no UDP como la telemetria que llega: por TCP se sabe con
 * CERTEZA que se entrego. Un apunte de viaje no se puede perder en silencio, y
 * con UDP habria que reinventar confirmaciones y reintentos. La telemetria si
 * puede perderse: llega otra al segundo siguiente.
 *
 * NO se llama desde la tarea de LVGL. Cada envio abre un socket y espera
 * respuesta, y bloquear ahi congelaria la pantalla varios segundos. Se hace en
 * una tarea corta de usar y tirar, y el resultado vuelve por lv_async_call, que
 * es la unica forma segura de tocar widgets desde fuera de LVGL.
 */
#include "p4_api.h"
#include "config_storage.h"
#include "esp_http_client.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "lvgl.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>

static const char *TAG = "p4_api";

#define P4_URL        "http://192.168.4.1/api/viaje"
/* 8 s: el AP esta a un metro, pero la P4 puede estar ocupada con la tarjeta
 * (el endpoint toma el cerrojo de la SD, hasta 3 s) y ademas puede tocarle
 * reasociarse. Corto se traduciria en fallos falsos. */
#define P4_TIMEOUT_MS 8000

/* Trabajo de un envio. Se reserva en el heap y lo libera la tarea al terminar:
 * quien llama no espera y no puede ser el dueño de esta memoria. */
typedef struct {
    char           cuerpo[192];
    p4_api_done_cb cb;
    bool           ok;
    int            estado;      /* codigo HTTP, o 0 si ni siquiera conecto */
} trabajo_t;

/* Vuelta al hilo de LVGL para avisar del resultado. */
static void avisar_cb(void *arg)
{
    trabajo_t *t = (trabajo_t *)arg;
    if (t->cb) t->cb(t->ok, t->estado);
    free(t);
}

/* El POST de verdad. BLOQUEA hasta tener respuesta o agotar el plazo, asi que
 * NO se llama desde la tarea de LVGL: la usa el repartidor de la cola, que
 * tiene la suya. */
bool p4_api_post(const char *cuerpo, int *estado_out)
{
    if (estado_out) *estado_out = 0;

    /* Las credenciales se leen en cada envio y no se cachean: el usuario puede
     * corregirlas en Ajustes entre un intento y el siguiente, y con una copia
     * en RAM seguiria fallando sin entender por que. */
    char user[33] = {0}, pass[65] = {0};
    size_t ul = sizeof(user), pl = sizeof(pass);
    bool hay_creds = (load_portal_creds(user, &ul, pass, &pl) == ESP_OK) && user[0];

    esp_http_client_config_t cfg = {
        .url            = P4_URL,
        .method         = HTTP_METHOD_POST,
        .timeout_ms     = P4_TIMEOUT_MS,
        .auth_type      = hay_creds ? HTTP_AUTH_TYPE_BASIC : HTTP_AUTH_TYPE_NONE,
        .username       = hay_creds ? user : NULL,
        .password       = hay_creds ? pass : NULL,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) {
        ESP_LOGE(TAG, "no puedo crear el cliente HTTP");
        return false;
    }

    esp_http_client_set_header(c, "Content-Type", "application/json");
    esp_http_client_set_post_field(c, cuerpo, strlen(cuerpo));

    bool ok = false;
    esp_err_t err = esp_http_client_perform(c);
    if (err == ESP_OK) {
        int estado = esp_http_client_get_status_code(c);
        if (estado_out) *estado_out = estado;
        /* Solo 2xx cuenta como entregado. Un 401 (credenciales), un 409 (ya
         * habia viaje abierto) o un 501 (todavia no implementado) NO son
         * exito: si se dieran por buenos, el apunte se daria por guardado sin
         * estarlo. */
        ok = (estado >= 200 && estado < 300);
        ESP_LOGI(TAG, "%s -> HTTP %d", cuerpo, estado);
    } else {
        ESP_LOGW(TAG, "no se pudo entregar: %s", esp_err_to_name(err));
    }
    esp_http_client_cleanup(c);
    return ok;
}

static void envio_task(void *arg)
{
    trabajo_t *t = (trabajo_t *)arg;
    t->ok = p4_api_post(t->cuerpo, &t->estado);
    lv_async_call(avisar_cb, t);
    vTaskDelete(NULL);
}

static bool lanzar(const char *cuerpo, p4_api_done_cb cb)
{
    trabajo_t *t = calloc(1, sizeof(trabajo_t));
    if (!t) return false;
    snprintf(t->cuerpo, sizeof(t->cuerpo), "%s", cuerpo);
    t->cb = cb;
    /* 6 KB: el cliente HTTP con su buffer de cabeceras no baja de unos 4. Con
     * menos se cuelga por desbordamiento de pila justo al conectar. */
    if (xTaskCreate(envio_task, "p4_api", 6144, t, 4, NULL) != pdPASS) {
        ESP_LOGE(TAG, "sin memoria para la tarea de envio");
        free(t);
        return false;
    }
    return true;
}

void p4_api_cuerpo_inicio(char *out, size_t n, uint32_t id,
                          const char *destino, uint32_t fecha_dias)
{
    snprintf(out, n,
             "{\"op\":\"inicio\",\"id\":%lu,\"destino\":\"%s\",\"fecha_dias\":%lu}",
             (unsigned long)id, destino, (unsigned long)fecha_dias);
}

void p4_api_cuerpo_fin(char *out, size_t n, uint32_t id, uint32_t eventos)
{
    snprintf(out, n, "{\"op\":\"fin\",\"id\":%lu,\"eventos\":%lu}",
             (unsigned long)id, (unsigned long)eventos);
}

void p4_api_cuerpo_fin_ajeno(char *out, size_t n, uint32_t id)
{
    snprintf(out, n, "{\"op\":\"fin\",\"id\":%lu}", (unsigned long)id);
}

void p4_api_cuerpo_descartar(char *out, size_t n, uint32_t id)
{
    snprintf(out, n, "{\"op\":\"descartar\",\"id\":%lu}", (unsigned long)id);
}

bool p4_api_viaje_inicio(uint32_t id, const char *destino, uint32_t fecha_dias,
                         p4_api_done_cb cb)
{
    char cuerpo[192];
    p4_api_cuerpo_inicio(cuerpo, sizeof(cuerpo), id, destino, fecha_dias);
    return lanzar(cuerpo, cb);
}


