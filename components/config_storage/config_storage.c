// config_storage.c
#include "config_storage.h"
#include <string.h>

#define BRIGHTNESS_NAMESPACE "display"
#define BRIGHTNESS_KEY       "brightness"
#define SCREENSAVER_NAMESPACE "screensaver"
#define SS_ENABLED_KEY        "enabled"
#define SS_BRIGHT_KEY         "brightness"
#define SS_TIMEOUT_KEY        "timeout"
#define RELAY_NAMESPACE       "relay"
#define RELAY_ENABLED_KEY     "enabled"
#define RELAY_COUNT_KEY       "count"
#define RELAY_PINS_KEY        "pins"
#define RELAY_LABELS_KEY      "labels"
#define RELAY_MAX_PINS        8
#define RELAY_UNUSED_PIN      0xFF
#define TILT_NAMESPACE        "tilt"
#define TILT_PITCH_KEY        "pitch_off"
#define TILT_ROLL_KEY         "roll_off"
#define WIFI_NAMESPACE        "wifi"
#define WIFI_SSID_KEY         "ssid"
#define WIFI_PASS_KEY         "password"
#define TRIP_NAMESPACE        "viaje"
#define TRIP_ACTIVE_KEY       "activo"
#define TRIP_DESTINO_KEY      "destino"
#define TRIP_SEQ_KEY          "seq"
#define TRIP_NEVENTOS_KEY     "n_ev"
/* Credenciales del PORTAL de la P4 (no del Wi-Fi): desde el 21-ago-2026 el
 * portal exige Basic Auth tambien en el nivel abierto, y el satelite escribe
 * en el. Se tecleaan una vez en Ajustes; se leen en la P4, Ajustes -> Wi-Fi. */
#define PORTAL_USER_KEY       "http_user"
#define PORTAL_PASS_KEY       "http_pass"
#define PARADA_NAMESPACE      "parada"
#define PARADA_ABIERTA_KEY    "abierta"
#define PARADA_LUGAR_KEY      "lugar"
#define PARADA_INICIO_KEY     "inicio_ts"
#define PARADA_COBRO_KEY      "cobro"
#define PARADA_MONEDA_KEY     "moneda"
#define PARADA_PRECIO_KEY     "precio"

esp_err_t load_brightness(uint8_t *brightness_out) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(BRIGHTNESS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_u8(h, BRIGHTNESS_KEY, brightness_out);
    if (err != ESP_OK) {
        *brightness_out = 5; // default value
        nvs_set_u8(h, BRIGHTNESS_KEY, *brightness_out);
        nvs_commit(h);
    }
    nvs_close(h);
    return ESP_OK;
}

esp_err_t save_brightness(uint8_t brightness) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(BRIGHTNESS_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, BRIGHTNESS_KEY, brightness);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t load_screensaver_settings(bool *enabled, uint8_t *brightness, uint16_t *timeout) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(SCREENSAVER_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    uint8_t en = 1, bright = 1;
    uint16_t tout = 10;
    nvs_get_u8(h, SS_ENABLED_KEY, &en);
    nvs_get_u8(h, SS_BRIGHT_KEY, &bright);
    nvs_get_u16(h, SS_TIMEOUT_KEY, &tout);

    if (enabled) *enabled = en;
    if (brightness) *brightness = bright;
    if (timeout) *timeout = tout;

    // Save defaults if not present
    nvs_set_u8(h, SS_ENABLED_KEY, en);
    nvs_set_u8(h, SS_BRIGHT_KEY, bright);
    nvs_set_u16(h, SS_TIMEOUT_KEY, tout);
    nvs_commit(h);
    nvs_close(h);
    return ESP_OK;
}

esp_err_t save_screensaver_settings(bool enabled, uint8_t brightness, uint16_t timeout) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(SCREENSAVER_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    nvs_set_u8(h, SS_ENABLED_KEY, enabled ? 1 : 0);
    nvs_set_u8(h, SS_BRIGHT_KEY, brightness);
    nvs_set_u16(h, SS_TIMEOUT_KEY, timeout);
    err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t load_relay_config(bool *enabled_out,
                            uint8_t *count_out,
                            uint8_t *pins_out,
                            char (*labels_out)[20],
                            size_t max_pins)
{
    if (enabled_out == NULL || count_out == NULL || pins_out == NULL || max_pins == 0) {
        return ESP_ERR_INVALID_ARG;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(RELAY_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }

    bool changed = false;

    uint8_t en = 1;
    esp_err_t tmp = nvs_get_u8(h, RELAY_ENABLED_KEY, &en);
    if (tmp != ESP_OK) {
        en = 1;
        nvs_set_u8(h, RELAY_ENABLED_KEY, en);
        changed = true;
    }

    uint8_t count = 0;
    tmp = nvs_get_u8(h, RELAY_COUNT_KEY, &count);
    if (tmp != ESP_OK) {
        count = 0;
        nvs_set_u8(h, RELAY_COUNT_KEY, count);
        changed = true;
    }

    if (count > RELAY_MAX_PINS) {
        count = RELAY_MAX_PINS;
        nvs_set_u8(h, RELAY_COUNT_KEY, count);
        changed = true;
    }

    uint8_t stored_pins[RELAY_MAX_PINS];
    memset(stored_pins, RELAY_UNUSED_PIN, sizeof(stored_pins));
    size_t blob_size = sizeof(stored_pins);
    tmp = nvs_get_blob(h, RELAY_PINS_KEY, stored_pins, &blob_size);
    if (tmp != ESP_OK || blob_size != sizeof(stored_pins)) {
        memset(stored_pins, RELAY_UNUSED_PIN, sizeof(stored_pins));
        nvs_set_blob(h, RELAY_PINS_KEY, stored_pins, sizeof(stored_pins));
        changed = true;
    }

    char stored_labels[RELAY_MAX_PINS][20];
    size_t labels_blob_size = sizeof(stored_labels);
    tmp = nvs_get_blob(h, RELAY_LABELS_KEY, stored_labels, &labels_blob_size);
    if (tmp != ESP_OK || labels_blob_size != sizeof(stored_labels)) {
        /* Initialize empty labels */
        for (size_t i = 0; i < RELAY_MAX_PINS; ++i) stored_labels[i][0] = '\0';
        nvs_set_blob(h, RELAY_LABELS_KEY, stored_labels, sizeof(stored_labels));
        changed = true;
    }

    if (changed) {
        nvs_commit(h);
    }

    nvs_close(h);

    if (count > max_pins) {
        count = (uint8_t)max_pins;
    }

    for (size_t i = 0; i < max_pins; ++i) {
        if (i < count) {
            pins_out[i] = stored_pins[i];
        } else {
            pins_out[i] = RELAY_UNUSED_PIN;
        }
        if (labels_out != NULL) {
            if (i < count) {
                strncpy(labels_out[i], stored_labels[i], 20);
                labels_out[i][19] = '\0';
            } else {
                labels_out[i][0] = '\0';
            }
        }
    }

    *enabled_out = (en != 0);
    *count_out = count;
    return ESP_OK;
}

esp_err_t save_relay_config(bool enabled,
                            const uint8_t *pins,
                            const char (*labels)[20],
                            uint8_t count)
{
    if (count > RELAY_MAX_PINS) {
        count = RELAY_MAX_PINS;
    }

    nvs_handle_t h;
    esp_err_t err = nvs_open(RELAY_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        return err;
    }

    uint8_t stored_pins[RELAY_MAX_PINS];
    for (size_t i = 0; i < RELAY_MAX_PINS; ++i) {
        stored_pins[i] = RELAY_UNUSED_PIN;
    }

    char stored_labels[RELAY_MAX_PINS][20];
    for (size_t i = 0; i < RELAY_MAX_PINS; ++i) stored_labels[i][0] = '\0';

    if (pins != NULL) {
        size_t copy_count = count;
        if (copy_count > RELAY_MAX_PINS) {
            copy_count = RELAY_MAX_PINS;
        }
        memcpy(stored_pins, pins, copy_count);
    }

    if (labels != NULL) {
        size_t copy_count = count;
        if (copy_count > RELAY_MAX_PINS) copy_count = RELAY_MAX_PINS;
        for (size_t i = 0; i < copy_count; ++i) {
            strncpy(stored_labels[i], labels[i], 20);
            stored_labels[i][19] = '\0';
        }
    }

    err = nvs_set_u8(h, RELAY_ENABLED_KEY, enabled ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_u8(h, RELAY_COUNT_KEY, count);
    if (err == ESP_OK) err = nvs_set_blob(h, RELAY_PINS_KEY, stored_pins, sizeof(stored_pins));
    if (err == ESP_OK) err = nvs_set_blob(h, RELAY_LABELS_KEY, stored_labels, sizeof(stored_labels));
    if (err == ESP_OK) err = nvs_commit(h);

    nvs_close(h);
    return err;
}

esp_err_t load_tilt_calibration(int16_t *pitch_offset_centi, int16_t *roll_offset_centi)
{
    if (!pitch_offset_centi || !roll_offset_centi) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(TILT_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        /* Namespace aun no existe -> sin calibrar todavia, no es un error. */
        *pitch_offset_centi = 0;
        *roll_offset_centi  = 0;
        return ESP_OK;
    }

    int16_t p = 0, r = 0;
    nvs_get_i16(h, TILT_PITCH_KEY, &p);
    nvs_get_i16(h, TILT_ROLL_KEY, &r);
    nvs_close(h);

    *pitch_offset_centi = p;
    *roll_offset_centi  = r;
    return ESP_OK;
}

esp_err_t save_tilt_calibration(int16_t pitch_offset_centi, int16_t roll_offset_centi)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(TILT_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_i16(h, TILT_PITCH_KEY, pitch_offset_centi);
    if (err == ESP_OK) err = nvs_set_i16(h, TILT_ROLL_KEY, roll_offset_centi);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t load_trip_active(bool *active_out)
{
    if (!active_out) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(TRIP_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        /* Namespace aun no existe -> nunca se ha iniciado un viaje. No es un
         * error: el estado de partida es "sin viaje". */
        *active_out = false;
        return ESP_OK;
    }

    uint8_t v = 0;
    nvs_get_u8(h, TRIP_ACTIVE_KEY, &v);
    nvs_close(h);

    *active_out = (v != 0);
    return ESP_OK;
}

esp_err_t save_trip_active(bool active)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(TRIP_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, TRIP_ACTIVE_KEY, active ? 1 : 0);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

/* Destino del viaje en curso. Es lo que da nombre a la carpeta en la SD de la
 * P4, asi que se guarda aqui tambien: si la 3.5" se reinicia a media entrega,
 * tiene que poder repetir el mismo nombre y no crear una carpeta nueva. */
esp_err_t load_trip_destino(char *out, size_t *len)
{
    if (!out || !len) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(TRIP_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_str(h, TRIP_DESTINO_KEY, out, len);
    nvs_close(h);
    return err;
}

esp_err_t save_trip_destino(const char *destino)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(TRIP_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, TRIP_DESTINO_KEY, destino ? destino : "");
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

/* Contador de apuntes, CRECIENTE Y SIN HUECOS: es lo que permite a la P4
 * descartar duplicados cuando un reintento llega dos veces. Persistente porque
 * la 3.5" se apaga con el contacto constantemente. */
uint32_t next_trip_seq(void)
{
    nvs_handle_t h;
    uint32_t v = 0;
    if (nvs_open(TRIP_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return 0;
    nvs_get_u32(h, TRIP_SEQ_KEY, &v);
    v++;
    nvs_set_u32(h, TRIP_SEQ_KEY, v);
    nvs_commit(h);
    nvs_close(h);
    return v;
}

/* Cuantos apuntes ha GENERADO este viaje, contando el inicio.
 *
 * Se cuenta lo generado y NO lo entregado, y esa es la clave: si un apunte no
 * llega a encolarse (cola llena, fallo de NVS), el contador sube igual y la P4
 * vera que le faltan. Si contaramos solo lo encolado, un apunte perdido cuadraria
 * las cuentas y el viaje se daria por completo sin serlo -- justo lo que este
 * contador existe para impedir. */
void trip_eventos_reset(void)
{
    nvs_handle_t h;
    if (nvs_open(TRIP_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return;
    nvs_set_u32(h, TRIP_NEVENTOS_KEY, 1);   /* el inicio ya cuenta */
    nvs_commit(h);
    nvs_close(h);
}

uint32_t trip_eventos_inc(void)
{
    nvs_handle_t h;
    uint32_t v = 0;
    if (nvs_open(TRIP_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) return 0;
    nvs_get_u32(h, TRIP_NEVENTOS_KEY, &v);
    v++;
    nvs_set_u32(h, TRIP_NEVENTOS_KEY, v);
    nvs_commit(h);
    nvs_close(h);
    return v;
}

esp_err_t load_portal_creds(char *user_out, size_t *user_len,
                            char *pass_out, size_t *pass_len)
{
    if (!user_out || !user_len || !pass_out || !pass_len) return ESP_ERR_INVALID_ARG;
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return err;
    err = nvs_get_str(h, PORTAL_USER_KEY, user_out, user_len);
    if (err == ESP_OK) err = nvs_get_str(h, PORTAL_PASS_KEY, pass_out, pass_len);
    nvs_close(h);
    return err;
}

esp_err_t save_portal_creds(const char *user, const char *pass)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_str(h, PORTAL_USER_KEY, user ? user : "");
    if (err == ESP_OK) err = nvs_set_str(h, PORTAL_PASS_KEY, pass ? pass : "");
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t load_parada_abierta(parada_abierta_t *out)
{
    if (!out) return ESP_ERR_INVALID_ARG;
    memset(out, 0, sizeof(*out));

    nvs_handle_t h;
    esp_err_t err = nvs_open(PARADA_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) return ESP_OK;   /* nunca hubo parada; no es un error */

    uint8_t abierta = 0;
    nvs_get_u8(h, PARADA_ABIERTA_KEY, &abierta);
    if (abierta) {
        nvs_get_u8(h, PARADA_LUGAR_KEY, &out->lugar);
        nvs_get_u32(h, PARADA_INICIO_KEY, &out->epoch_inicio);
        nvs_get_u8(h, PARADA_COBRO_KEY, &out->cobro);
        nvs_get_u8(h, PARADA_MONEDA_KEY, &out->moneda);
        size_t len = sizeof(out->precio);
        if (nvs_get_str(h, PARADA_PRECIO_KEY, out->precio, &len) != ESP_OK) {
            out->precio[0] = '\0';
        }
    }
    nvs_close(h);

    /* Sin hora de inicio no hay forma de contar el tiempo, asi que una parada
     * asi se da por no abierta en vez de quedarse colgada para siempre
     * preguntando lo que no se puede responder. */
    out->abierta = (abierta != 0) && (out->epoch_inicio > 0);
    return ESP_OK;
}

esp_err_t save_parada_abierta(const parada_abierta_t *p)
{
    if (!p) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(PARADA_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_set_u8(h, PARADA_ABIERTA_KEY, p->abierta ? 1 : 0);
    if (err == ESP_OK) err = nvs_set_u8(h, PARADA_LUGAR_KEY, p->lugar);
    if (err == ESP_OK) err = nvs_set_u32(h, PARADA_INICIO_KEY, p->epoch_inicio);
    if (err == ESP_OK) err = nvs_set_u8(h, PARADA_COBRO_KEY, p->cobro);
    if (err == ESP_OK) err = nvs_set_u8(h, PARADA_MONEDA_KEY, p->moneda);
    if (err == ESP_OK) err = nvs_set_str(h, PARADA_PRECIO_KEY, p->precio);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t clear_parada_abierta(void)
{
    nvs_handle_t h;
    esp_err_t err = nvs_open(PARADA_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    /* Basta con bajar la bandera: el resto de campos se reescriben enteros la
     * proxima vez que se abra una parada. */
    err = nvs_set_u8(h, PARADA_ABIERTA_KEY, 0);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}

esp_err_t load_wifi_config(char *ssid_out, size_t *ssid_len,
                           char *pass_out, size_t *pass_len)
{
    if (!ssid_out || !ssid_len || !pass_out || !pass_len) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READONLY, &h);
    if (err != ESP_OK) {
        /* Namespace aun no existe -> primer arranque, NVS vacia. */
        return err;
    }

    err = nvs_get_str(h, WIFI_SSID_KEY, ssid_out, ssid_len);
    if (err != ESP_OK) {
        nvs_close(h);
        return err;
    }
    err = nvs_get_str(h, WIFI_PASS_KEY, pass_out, pass_len);
    nvs_close(h);
    return err;
}

esp_err_t save_wifi_config(const char *ssid, const char *pass)
{
    if (!ssid || !pass) return ESP_ERR_INVALID_ARG;

    nvs_handle_t h;
    esp_err_t err = nvs_open(WIFI_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;

    err = nvs_set_str(h, WIFI_SSID_KEY, ssid);
    if (err == ESP_OK) err = nvs_set_str(h, WIFI_PASS_KEY, pass);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
