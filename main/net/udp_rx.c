/* udp_rx.c - Receiver UDP de 35cabina, asociado al SoftAP de la P4 como STA.
 *
 * Portado de ~/joint/victron_mini/main/net/udp_rx.c (mismo protocolo,
 * mismo patron de reconexion).
 *
 * SSID/password NO estan hardcodeados en firmware: se guardan en NVS
 * (config_storage.c, load/save_wifi_config) para poder cambiar de P4 (ej.
 * la de repuesto para pruebas) desde la pantalla de Ajustes sin
 * reflashear. wifi_credentials.h (no versionado) solo se usa como valor
 * de fabrica la PRIMERA vez que arranca con NVS vacia -- el mismo
 * problema que ya sufrio victron_mini (password fija en firmware, sin
 * forma de cambiarla sin reflashear) queda resuelto aqui.
 *
 * Flujo:
 *  1. nvs/netif/event_loop init.
 *  2. Carga SSID/pass de NVS (o de wifi_credentials.h si es la primera vez).
 *  3. esp_wifi en modo STA, conecta al AP de la P4.
 *  4. Reconexion automatica si la pierde.
 *  5. Tras obtener IP por DHCP, arranca task que bind UDP en :4242 y
 *     llama data_model_update_from_msg con cada mini_msg_t valido.
 */
#include "udp_rx.h"
#include "mini_proto.h"
#include "wifi_credentials.h"
#include "config_storage.h"
#include "../data_model.h"
#include "esp_log.h"
#include "esp_mac.h"
#include "esp_wifi.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_crc.h"
#include "esp_timer.h"
#include "nvs_flash.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/event_groups.h"
#include "lwip/sockets.h"
#include <string.h>
#include <errno.h>

static const char *TAG = "udp_rx";

#define WIFI_BIT_CONNECTED  BIT0
#define WIFI_BIT_GOT_IP     BIT1

static EventGroupHandle_t s_wifi_events;
static uint32_t s_msgs_ok = 0;
static uint32_t s_msgs_bad = 0;
static esp_timer_handle_t s_reconnect_timer;
#define RECONNECT_DELAY_US (500 * 1000)

static char s_ssid[33];
static char s_pass[65];

static void load_or_init_credentials(void)
{
    size_t ssid_len = sizeof(s_ssid);
    size_t pass_len = sizeof(s_pass);
    esp_err_t err = load_wifi_config(s_ssid, &ssid_len, s_pass, &pass_len);
    if (err != ESP_OK) {
        ESP_LOGI(TAG, "Sin credenciales en NVS, usando valor de fabrica de wifi_credentials.h");
        strncpy(s_ssid, WIFI_CRED_SSID, sizeof(s_ssid) - 1);
        s_ssid[sizeof(s_ssid) - 1] = '\0';
        strncpy(s_pass, WIFI_CRED_PASS, sizeof(s_pass) - 1);
        s_pass[sizeof(s_pass) - 1] = '\0';
        save_wifi_config(s_ssid, s_pass);
    }
}

/* Lista las redes que ve la radio. Solo para diagnostico: cuando la P4 no
 * aparece, esto dice si el problema es que no esta emitiendo, que se llama de
 * otra forma o que su cifrado no pasa el filtro de wifi_configure_sta(). */
static void log_redes_visibles(void)
{
    /* Como mucho una vez por minuto: cada escaneo deja la radio ocupada un par
     * de segundos y aqui se entra cada 0.5 s mientras no haya red. */
    static int64_t ultimo_us = 0;
    int64_t ahora = esp_timer_get_time();
    if (ultimo_us != 0 && (ahora - ultimo_us) < 60LL * 1000000LL) return;
    ultimo_us = ahora;

    wifi_scan_config_t cfg = { .show_hidden = true };
    if (esp_wifi_scan_start(&cfg, true) != ESP_OK) return;

    uint16_t n = 0;
    esp_wifi_scan_get_ap_num(&n);
    if (n == 0) {
        ESP_LOGW(TAG, "Escaneo: NO se ve ninguna red. La P4 no esta emitiendo "
                      "o esta fuera de alcance.");
        return;
    }
    if (n > 12) n = 12;

    wifi_ap_record_t *aps = calloc(n, sizeof(*aps));
    if (!aps) return;
    if (esp_wifi_scan_get_ap_records(&n, aps) == ESP_OK) {
        ESP_LOGW(TAG, "Escaneo: %u redes visibles (busco '%s')", n, s_ssid);
        for (uint16_t i = 0; i < n; i++) {
            ESP_LOGW(TAG, "  '%s'  canal=%d  rssi=%d  authmode=%d%s",
                     (const char *)aps[i].ssid, aps[i].primary, aps[i].rssi,
                     aps[i].authmode,
                     strcmp((const char *)aps[i].ssid, s_ssid) == 0 ? "   <-- ES ESTA" : "");
        }
    }
    free(aps);
}

static void wifi_configure_sta(void)
{
    wifi_config_t wc = {0};
    strncpy((char *)wc.sta.ssid, s_ssid, sizeof(wc.sta.ssid));
    strncpy((char *)wc.sta.password, s_pass, sizeof(wc.sta.password));
    /* 'threshold' es el cifrado MINIMO que se acepta y va por el valor del enum,
     * asi que poner uno mas alto que el del AP hace que el escaneo lo descarte y
     * el sintoma sea un enganoso "reason=201 (NO_AP_FOUND)": parece que la red
     * no esta, cuando esta delante.
     *
     * Historia, porque costo una noche entera (21-ago-2026): el AP del 7" estuvo
     * ABIERTO y llamandose "ESP_<MAC>" desde julio -- el C6 llevaba el firmware
     * de fabrica de la placa, hablaba otro protocolo, la configuracion del AP le
     * llegaba vacia y levantaba el suyo por defecto. Se arreglo actualizando el
     * firmware del C6 desde el propio 7" (Ajustes -> Wi-Fi -> Actualizar radio),
     * y desde entonces el AP es "VictronConfig" con WPA2 de verdad.
     *
     * Por eso esto vuelve a WPA2_PSK: acepta el AP y rechaza redes abiertas o
     * WEP, que es de lo que protege. Si alguna vez reaparece el AP abierto, el
     * problema esta en el C6, NO aqui: no bajar esto sin mirar antes el log del
     * 7" ("AP en la radio: ssid=... authmode=..."). */
    wc.sta.threshold.authmode = WIFI_AUTH_WPA2_PSK;
    wc.sta.pmf_cfg.required = false;
    esp_wifi_set_config(WIFI_IF_STA, &wc);
    ESP_LOGI(TAG, "AP SSID='%s'", s_ssid);
}

void udp_rx_get_credentials(char *ssid_out, size_t ssid_len, char *pass_out, size_t pass_len)
{
    if (ssid_out && ssid_len > 0) {
        strncpy(ssid_out, s_ssid, ssid_len - 1);
        ssid_out[ssid_len - 1] = '\0';
    }
    if (pass_out && pass_len > 0) {
        strncpy(pass_out, s_pass, pass_len - 1);
        pass_out[pass_len - 1] = '\0';
    }
}

void udp_rx_set_credentials(const char *ssid, const char *pass)
{
    if (!ssid || !pass) return;
    strncpy(s_ssid, ssid, sizeof(s_ssid) - 1);
    s_ssid[sizeof(s_ssid) - 1] = '\0';
    strncpy(s_pass, pass, sizeof(s_pass) - 1);
    s_pass[sizeof(s_pass) - 1] = '\0';

    save_wifi_config(s_ssid, s_pass);
    wifi_configure_sta();
    ESP_LOGI(TAG, "Credenciales actualizadas, reconectando a '%s'", s_ssid);
    esp_wifi_disconnect();
    esp_wifi_connect();
}

/* Corre en la tarea del esp_timer, no en sys_evt: aqui si es seguro que el
 * reintento se retrase sin bloquear el procesado de otros eventos WiFi/IP. */
static void reconnect_timer_cb(void *arg)
{
    (void)arg;
    esp_wifi_connect();
}

static void on_wifi_event(void *arg, esp_event_base_t base, int32_t id, void *data)
{
    (void)arg; (void)data;
    if (base == WIFI_EVENT) {
        switch (id) {
            case WIFI_EVENT_STA_START:
                ESP_LOGI(TAG, "STA start");
                esp_wifi_connect();
                break;
            case WIFI_EVENT_STA_CONNECTED:
                ESP_LOGI(TAG, "Asociado a %s", s_ssid);
                xEventGroupSetBits(s_wifi_events, WIFI_BIT_CONNECTED);
                break;
            case WIFI_EVENT_STA_DISCONNECTED: {
                wifi_event_sta_disconnected_t *d = (wifi_event_sta_disconnected_t *)data;
                ESP_LOGW(TAG, "Desconectado reason=%d, siguiente intento en 0.5s",
                         d ? d->reason : -1);
                /* Con NO_AP_FOUND (201) el mensaje solo no basta: dice que no
                 * la encuentra, pero no si es que no esta, si esta con otro
                 * nombre o si la esta descartando por el cifrado. Se lista lo
                 * que ve, de vez en cuando para no llenar el log ni pasarse el
                 * dia escaneando (un escaneo bloquea la radio ~2 s). */
                if (d && d->reason == WIFI_REASON_NO_AP_FOUND) log_redes_visibles();
                xEventGroupClearBits(s_wifi_events, WIFI_BIT_CONNECTED | WIFI_BIT_GOT_IP);
                esp_timer_stop(s_reconnect_timer);   /* no-op si no estaba armado */
                esp_timer_start_once(s_reconnect_timer, RECONNECT_DELAY_US);
                break;
            }
            default: break;
        }
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        ip_event_got_ip_t *ev = (ip_event_got_ip_t *)data;
        ESP_LOGI(TAG, "IP: " IPSTR " GW: " IPSTR,
                 IP2STR(&ev->ip_info.ip), IP2STR(&ev->ip_info.gw));
        xEventGroupSetBits(s_wifi_events, WIFI_BIT_GOT_IP);
    }
}

static void rx_task(void *arg)
{
    (void)arg;

    xEventGroupWaitBits(s_wifi_events, WIFI_BIT_GOT_IP,
                        pdFALSE, pdTRUE, portMAX_DELAY);

    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) {
        ESP_LOGE(TAG, "socket() errno=%d", errno);
        vTaskDelete(NULL);
        return;
    }

    struct sockaddr_in bind_addr = {0};
    bind_addr.sin_family      = AF_INET;
    bind_addr.sin_port        = htons(MINI_PROTO_UDP_PORT);
    bind_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(sock, (struct sockaddr *)&bind_addr, sizeof(bind_addr)) < 0) {
        ESP_LOGE(TAG, "bind(:%d) errno=%d", MINI_PROTO_UDP_PORT, errno);
        close(sock);
        vTaskDelete(NULL);
        return;
    }
    ESP_LOGI(TAG, "Escuchando UDP :%d (sizeof(mini_msg_t)=%u)",
             MINI_PROTO_UDP_PORT, (unsigned)sizeof(mini_msg_t));

    uint8_t buf[256];
    struct sockaddr_in src;
    socklen_t slen = sizeof(src);

    for (;;) {
        int n = recvfrom(sock, buf, sizeof(buf), 0,
                         (struct sockaddr *)&src, &slen);
        if (n < 0) {
            ESP_LOGW(TAG, "recvfrom errno=%d", errno);
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }
        if (n != (int)sizeof(mini_msg_t)) {
            ESP_LOGW(TAG, "drop: tam %d != %u", n, (unsigned)sizeof(mini_msg_t));
            s_msgs_bad++;
            continue;
        }
        const mini_msg_t *msg = (const mini_msg_t *)buf;
        if (msg->version != MINI_PROTO_VERSION) {
            ESP_LOGW(TAG, "drop: version %u", msg->version);
            s_msgs_bad++;
            continue;
        }
        uint32_t expected = esp_crc32_le(0, buf, sizeof(mini_msg_t) - sizeof(uint32_t));
        if (expected != msg->crc32) {
            ESP_LOGW(TAG, "drop: crc 0x%08lx != 0x%08lx",
                     (unsigned long)msg->crc32, (unsigned long)expected);
            s_msgs_bad++;
            continue;
        }
        data_model_update_from_msg(msg);
        s_msgs_ok++;

        /* El reloj de la P4 es lo unico que permite contar lo que dura una
         * parada (esta pantalla no tiene RTC). Se avisa UNA vez cuando llega y
         * otra si se pierde, para que no haya que adivinar por que una parada
         * no se abre: sin fecha valida no se abre, y punto. */
        {
            static bool tenia_fecha = false;
            bool hay = (msg->epoch_local != 0);
            if (hay != tenia_fecha) {
                if (hay) {
                    /* epoch_local ya viene en hora local de la P4, asi que se
                     * desmonta a mano en vez de con localtime(), que aqui
                     * aplicaria un huso que esta pantalla no tiene puesto. */
                    uint32_t t = msg->epoch_local;
                    uint32_t seg = t % 86400u;
                    uint32_t dias = t / 86400u;
                    ESP_LOGI(TAG, "Reloj de la P4 recibido: dia %lu, hora %02lu:%02lu "
                                  "-> las paradas ya se pueden contar",
                             (unsigned long)dias,
                             (unsigned long)(seg / 3600), (unsigned long)((seg / 60) % 60));
                } else {
                    ESP_LOGW(TAG, "La P4 ha dejado de dar la hora: las paradas "
                                  "no se podran abrir ni cerrar");
                }
                tenia_fecha = hay;
            }
        }

        if ((s_msgs_ok % 10) == 1) {
            ESP_LOGI(TAG, "RX OK #%lu (bad=%lu) from %s soc=%d.%d V=%d.%02d I=%ld mA",
                     (unsigned long)s_msgs_ok, (unsigned long)s_msgs_bad,
                     inet_ntoa(src.sin_addr),
                     msg->shunt_soc_deci / 10, msg->shunt_soc_deci % 10,
                     msg->shunt_voltage_centi / 100, msg->shunt_voltage_centi % 100,
                     (long)msg->shunt_current_milli);
        }
    }
}

void udp_rx_start(void)
{
    load_or_init_credentials();

    ESP_ERROR_CHECK(esp_netif_init());
    ESP_ERROR_CHECK(esp_event_loop_create_default());
    esp_netif_create_default_wifi_sta();

    s_wifi_events = xEventGroupCreate();

    const esp_timer_create_args_t timer_args = {
        .callback = reconnect_timer_cb,
        .name = "wifi_reconnect",
    };
    ESP_ERROR_CHECK(esp_timer_create(&timer_args, &s_reconnect_timer));

    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    ESP_ERROR_CHECK(esp_wifi_init(&cfg));
    ESP_ERROR_CHECK(esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                 on_wifi_event, NULL));
    ESP_ERROR_CHECK(esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                 on_wifi_event, NULL));

    ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));
    ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
    wifi_configure_sta();
    ESP_ERROR_CHECK(esp_wifi_start());

    uint8_t mac[6];
    esp_wifi_get_mac(WIFI_IF_STA, mac);
    ESP_LOGI(TAG, "STA listo. MAC=" MACSTR " esperando IP por DHCP...", MAC2STR(mac));

    xTaskCreate(rx_task, "udp_rx", 4096, NULL, 4, NULL);
}
