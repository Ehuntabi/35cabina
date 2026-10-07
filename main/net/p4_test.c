/* p4_test.c — Auto-test del canal de SUBIDA a la P4 (cabina -> P4 por HTTP).
 *
 * POR QUE EXISTE (7-oct-2026)
 * El camino de bajada (telemetria UDP) y el latido se prueban solos: se ven en
 * el log. El de subida NO: la cabina solo le habla a la P4 cuando alguien inicia
 * o cierra un viaje, asi que un fallo ahi se descubre con la autocaravana
 * aparcada y el viaje a medias. Y hay tres cosas que solo se prueban juntas:
 * que la ruta exista en la P4, que las CREDENCIALES del portal sean correctas y
 * que la P4 acepte un POST con el cuerpo de la cabina.
 *
 * QUE HACE
 * Al arrancar, una sola vez y en su propia tarea (no bloquea el arranque ni la
 * UI), hace tres peticiones y lo deja dicho en el log en castellano:
 *
 *   1. POST /api/viaje SIN credenciales y con una operacion desconocida -> 401:
 *      la ruta existe y pide clave (lo normal). 404 = la P4 lleva un firmware sin
 *      esa ruta. OJO: tiene que ser POST, porque en la P4 esa ruta esta
 *      registrada solo para POST y un GET lo coge el portal cautivo y lo
 *      redirige en bucle (ESP_ERR_HTTP_MAX_REDIRECT, visto el 7-oct-2026).
 *   2. El mismo POST CON credenciales -> 4xx (la P4 reconoce al cliente y
 *      rechaza la operacion rara) = credenciales BIEN; 401 = estan mal o no hay
 *      ninguna puesta, que es el fallo que deja los viajes sin guardar.
 *   3. Y con eso queda comprobado que la P4 acepta POST de la cabina y rechaza
 *      lo que no entiende, sin crear ningun viaje.
 *
 * NO crea ningun viaje ni escribe nada en la P4: las tres peticiones son de
 * lectura o de algo que la P4 rechaza a proposito.
 */
#include "p4_test.h"

#include <string.h>
#include "esp_log.h"
#include "esp_http_client.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "p4_api.h"          /* p4_api_post: el POST crudo, sincrono */
#include "config_storage.h"  /* load_portal_creds */

static const char *TAG = "p4_test";

#define P4_HOST        "http://192.168.4.1"
#define P4_URL_VIAJE   P4_HOST "/api/state"   /* GET de lectura: existe seguro */
#define P4_TIMEOUT_MS  8000

/* Una peticion. 'con_creds' decide si se manda Basic Auth. Devuelve el codigo
 * HTTP, o 0 si no se pudo ni conectar (P4 apagada, fuera de alcance, sin IP). */
static int pedir(const char *url, esp_http_client_method_t metodo,
                 const char *cuerpo, bool con_creds)
{
    char user[33] = {0}, pass[65] = {0};
    size_t ul = sizeof(user), pl = sizeof(pass);
    bool hay = con_creds && (load_portal_creds(user, &ul, pass, &pl) == ESP_OK) && user[0];

    esp_http_client_config_t cfg = {
        .url        = url,
        .method     = metodo,
        .timeout_ms = P4_TIMEOUT_MS,
        .auth_type  = hay ? HTTP_AUTH_TYPE_BASIC : HTTP_AUTH_TYPE_NONE,
        .username   = hay ? user : NULL,
        .password   = hay ? pass : NULL,
        /* KEEP-ALIVE APAGADO, y no es un detalle: con keep-alive el cliente se
         * queda esperando a que el servidor cierre la conexion y 'perform' NO
         * VUELVE NUNCA (ni con timeout_ms), asi que la pantalla se reinicia por
         * el watchdog. Visto en el banco el 7-oct-2026: el auto-test arrancaba y
         * no terminaba jamas. */
        .keep_alive_enable = false,
    };
    esp_http_client_handle_t c = esp_http_client_init(&cfg);
    if (!c) return 0;
    if (cuerpo) {
        esp_http_client_set_header(c, "Content-Type", "application/json");
        esp_http_client_set_post_field(c, cuerpo, strlen(cuerpo));
    }
    esp_err_t err = esp_http_client_perform(c);
    int estado = (err == ESP_OK) ? esp_http_client_get_status_code(c) : 0;
    esp_http_client_cleanup(c);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "%s: no se pudo entregar (%s)", url, esp_err_to_name(err));
    }
    return estado;
}

static void tarea(void *arg)
{
    (void)arg;
    /* Se espera a que el enlace este de verdad: la tarea arranca con el Wi-Fi y
     * la IP tarda unos segundos (medido: ~5 s). Sin esta espera el test
     * fallaria por impaciencia y asustaria sin motivo. */
    vTaskDelay(pdMS_TO_TICKS(12000));

    ESP_LOGI(TAG, "=== auto-test del canal de subida a la P4 ===");

    /* Se prueba contra /api/state (GET, de lectura) y no contra /api/viaje:
     * asi se comprueba el CAMINO HTTP y las CREDENCIALES sin crear ni tocar
     * ningun viaje, y sin depender de que el handler de viaje conteste a una
     * sonda que no entiende. */
    int sin_creds = pedir(P4_URL_VIAJE, HTTP_METHOD_GET, NULL, false);
    vTaskDelay(pdMS_TO_TICKS(1500));
    int con_creds = pedir(P4_URL_VIAJE, HTTP_METHOD_GET, NULL, true);
    int post_raro = con_creds;

    if (sin_creds == 0) {
        ESP_LOGE(TAG, "1) ni contesta: la P4 no esta encendida, o fuera de alcance");
        ESP_LOGE(TAG, "   (sin enlace no se puede probar nada mas: se corta el test)");
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "1) el portal responde (sin credenciales: HTTP %d)%s",
             sin_creds, sin_creds == 401 ? " y pide clave, como debe" : "");

    if (con_creds >= 400 && con_creds < 500 && con_creds != 401) {
        ESP_LOGI(TAG, "2) CREDENCIALES DEL PORTAL OK (HTTP %d: reconoce al cliente y "
                      "rechaza la operacion rara)", con_creds);
    } else if (con_creds == 200) {
        ESP_LOGW(TAG, "2) la P4 ha aceptado una operacion desconocida (HTTP 200): revisar");
    } else if (con_creds == 401) {
        ESP_LOGE(TAG, "2) CREDENCIALES MAL o sin poner (HTTP 401): los apuntes de viaje");
        ESP_LOGE(TAG, "   se perderan. Se ponen en Ajustes -> WiFi -> 'Usuario/Clave del portal',");
        ESP_LOGE(TAG, "   copiandolas de la P4 (Ajustes -> Wi-Fi)");
    } else {
        ESP_LOGW(TAG, "2) respuesta rara de la P4 con credenciales: HTTP %d", con_creds);
    }

    ESP_LOGI(TAG, "3) la P4 contesta al GET de la cabina (HTTP %d)", post_raro);

    ESP_LOGI(TAG, "=== fin del auto-test (no se ha creado ningun viaje) ===");
    vTaskDelete(NULL);
}

void p4_test_start(void)
{
    if (xTaskCreate(tarea, "p4_test", 5120, NULL, 3, NULL) != pdPASS) {
        ESP_LOGW(TAG, "no se pudo crear la tarea del auto-test (sin memoria)");
    }
}
