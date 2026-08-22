/* viaje_cola.c — cola de apuntes pendientes de entregar a la P4.
 *
 * Por que existe: la 3.5" se apaga con el contacto CONSTANTEMENTE, y las
 * paradas y los repostajes se apuntan justo antes de apagar. Sin cola, un
 * apunte hecho con la P4 caida se perderia sin que nadie se entere. Con la P4
 * encendida casi siempre (arranca antes que esta pantalla), la cola pasa de ser
 * el camino habitual a ser la red de seguridad -- pero es justo la red la que
 * tiene que aguantar.
 *
 * En NVS y no en RAM, por lo mismo: quitar el contacto no puede tirar nada.
 *
 * ORDEN GARANTIZADO. Se envia siempre desde la cabeza y no se pasa a la
 * siguiente hasta que la actual se entrega. El "fin" de viaje entra como uno
 * mas, asi que NUNCA adelanta a los registros que se apuntaron antes que el:
 * un viaje no puede cerrarse en la P4 con apuntes suyos todavia por llegar.
 */
#include "viaje_cola.h"
#include "p4_api.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "nvs.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "viaje_cola";

#define NS          "vcola"
#define K_CABEZA    "cabeza"     /* indice del proximo a enviar */
#define K_COLA      "cola"       /* indice donde se escribira el siguiente */

/* 64 entradas. Un viaje largo apuntando a mano no llega a tanto ni de lejos, y
 * la NVS de este aparato no es infinita. Al llenarse se AVISA en vez de tirar
 * nada en silencio, que es el fallo que esta cola existe para evitar. */
#define CAPACIDAD   64

/* Un envio completo cabe de sobra: el mas largo previsto (una parada con todos
 * sus servicios) ronda los 300 bytes. */
#define CUERPO_MAX  384

/* Cada cuanto se reintenta la cabeza cuando hay algo pendiente. 15 s: lo
 * bastante seguido para que al encender la P4 se vacie enseguida, y lo bastante
 * espaciado para no estar dando la lata a un AP que no esta. */
#define REINTENTO_MS 15000

static SemaphoreHandle_t s_mutex;
static viaje_cola_cambio_cb s_cambio_cb;

/* ── NVS ──────────────────────────────────────────────────────────────────── */

static void indices_leer(uint32_t *cabeza, uint32_t *cola)
{
    *cabeza = 0; *cola = 0;
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return;
    nvs_get_u32(h, K_CABEZA, cabeza);
    nvs_get_u32(h, K_COLA, cola);
    nvs_close(h);
}

static void indices_escribir(uint32_t cabeza, uint32_t cola)
{
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u32(h, K_CABEZA, cabeza);
    nvs_set_u32(h, K_COLA, cola);
    nvs_commit(h);
    nvs_close(h);
}

/* Los indices solo CRECEN; la clave es el indice modulo la capacidad. Asi no
 * hay que renumerar nada al sacar de la cabeza, que seria reescribir la NVS
 * entera por cada envio. */
static void clave_de(uint32_t idx, char *out, size_t n)
{
    snprintf(out, n, "q%lu", (unsigned long)(idx % CAPACIDAD));
}

/* ── API ──────────────────────────────────────────────────────────────────── */

size_t viaje_cola_pendientes(void)
{
    uint32_t cabeza, cola;
    indices_leer(&cabeza, &cola);
    return (size_t)(cola - cabeza);
}

static void avisar_cambio(void)
{
    if (s_cambio_cb) s_cambio_cb(viaje_cola_pendientes());
}

bool viaje_cola_push(const char *cuerpo)
{
    if (!cuerpo || !cuerpo[0]) return false;
    if (strlen(cuerpo) >= CUERPO_MAX) {
        ESP_LOGE(TAG, "apunte demasiado largo (%u bytes), NO se encola",
                 (unsigned)strlen(cuerpo));
        return false;
    }

    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint32_t cabeza, cola;
    indices_leer(&cabeza, &cola);

    if (cola - cabeza >= CAPACIDAD) {
        xSemaphoreGive(s_mutex);
        ESP_LOGE(TAG, "cola LLENA (%d): no se encola nada mas", CAPACIDAD);
        return false;
    }

    char clave[16];
    clave_de(cola, clave, sizeof(clave));

    nvs_handle_t h;
    if (nvs_open(NS, NVS_READWRITE, &h) != ESP_OK) {
        xSemaphoreGive(s_mutex);
        return false;
    }
    esp_err_t e = nvs_set_str(h, clave, cuerpo);
    if (e == ESP_OK) e = nvs_commit(h);
    nvs_close(h);
    if (e != ESP_OK) {
        xSemaphoreGive(s_mutex);
        ESP_LOGE(TAG, "no puedo guardar el apunte: %s", esp_err_to_name(e));
        return false;
    }

    indices_escribir(cabeza, cola + 1);
    xSemaphoreGive(s_mutex);

    ESP_LOGI(TAG, "encolado #%lu (%u pendientes): %s",
             (unsigned long)cola, (unsigned)(cola + 1 - cabeza), cuerpo);
    avisar_cambio();
    return true;
}

/* Saca la cabeza. Solo se llama tras una entrega CONFIRMADA. */
static void descartar_cabeza(void)
{
    xSemaphoreTake(s_mutex, portMAX_DELAY);
    uint32_t cabeza, cola;
    indices_leer(&cabeza, &cola);
    if (cabeza < cola) {
        char clave[16];
        clave_de(cabeza, clave, sizeof(clave));
        nvs_handle_t h;
        if (nvs_open(NS, NVS_READWRITE, &h) == ESP_OK) {
            nvs_erase_key(h, clave);
            nvs_commit(h);
            nvs_close(h);
        }
        indices_escribir(cabeza + 1, cola);
    }
    xSemaphoreGive(s_mutex);
    avisar_cambio();
}

static bool leer_cabeza(char *out, size_t n)
{
    uint32_t cabeza, cola;
    indices_leer(&cabeza, &cola);
    if (cabeza >= cola) return false;

    char clave[16];
    clave_de(cabeza, clave, sizeof(clave));
    nvs_handle_t h;
    if (nvs_open(NS, NVS_READONLY, &h) != ESP_OK) return false;
    size_t len = n;
    esp_err_t e = nvs_get_str(h, clave, out, &len);
    nvs_close(h);
    return e == ESP_OK;
}

/* ── El repartidor ────────────────────────────────────────────────────────── */

static void reparto_task(void *arg)
{
    (void)arg;
    char cuerpo[CUERPO_MAX];

    while (1) {
        if (!leer_cabeza(cuerpo, sizeof(cuerpo))) {
            /* Cola vacia: dormir el ciclo entero. No hay nada que hacer y
             * despertarse mas a menudo solo gasta bateria. */
            vTaskDelay(pdMS_TO_TICKS(REINTENTO_MS));
            continue;
        }

        int estado = 0;
        bool ok = p4_api_post(cuerpo, &estado);

        if (ok) {
            descartar_cabeza();
            /* Sin espera: si hay mas, se sigue vaciando de seguido. Que la P4
             * este respondiendo es justo el momento de aprovechar. */
            continue;
        }

        /* 4xx que NO son "vuelve luego": el apunte esta mal formado y
         * reintentarlo eternamente atascaria la cola entera detras de el. Se
         * tira, pero dejando constancia bien visible en el log.
         *
         * Tres se EXCLUYEN porque significan "ahora no" y no "esto no vale":
         *   401 credenciales mal puestas -> se arreglan en Ajustes y entonces
         *       el mismo apunte entra bien. Tirarlo seria perder un repostaje
         *       por un dedazo.
         *   409 no hay viaje abierto todavia en la P4 -> el inicio puede estar
         *       aun por delante en esta misma cola.
         *   408 / 429 son "vuelve luego" por definicion. */
        if (estado >= 400 && estado < 500 && estado != 401 && estado != 408 &&
            estado != 409 && estado != 429) {
            ESP_LOGE(TAG, "la P4 rechaza el apunte con %d, lo DESCARTO: %s",
                     estado, cuerpo);
            descartar_cabeza();
            continue;
        }

        vTaskDelay(pdMS_TO_TICKS(REINTENTO_MS));
    }
}

void viaje_cola_init(viaje_cola_cambio_cb cb)
{
    s_mutex = xSemaphoreCreateMutex();
    s_cambio_cb = cb;

    size_t n = viaje_cola_pendientes();
    if (n) ESP_LOGW(TAG, "arranco con %u apuntes sin entregar del encendido anterior",
                    (unsigned)n);

    /* 6 KB por el cliente HTTP, igual que la tarea de envio directo que
     * sustituye. Prioridad 4: por debajo de LVGL, esto nunca corre prisa. */
    xTaskCreate(reparto_task, "viaje_cola", 6144, NULL, 4, NULL);
    avisar_cambio();
}
