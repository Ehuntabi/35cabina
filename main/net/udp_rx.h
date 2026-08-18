/* udp_rx.h - Receiver UDP de 35cabina.
 *
 * Configura el ESP32-S3 como STA, se asocia al SoftAP de la P4 y escucha
 * paquetes UDP broadcast en MINI_PROTO_UDP_PORT con el payload mini_msg_t.
 *
 * SSID/password se guardan en NVS (cambiables desde Ajustes sin
 * reflashear); wifi_credentials.h (no versionado, ver
 * wifi_credentials.h.example y .gitignore) solo es el valor de fabrica
 * del primer arranque.
 */
#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void udp_rx_start(void);

/* Copia las credenciales actuales (para prefijar la pantalla de Ajustes). */
void udp_rx_get_credentials(char *ssid_out, size_t ssid_len, char *pass_out, size_t pass_len);

/* Guarda nuevas credenciales en NVS y fuerza una reconexion inmediata. */
void udp_rx_set_credentials(const char *ssid, const char *pass);

#ifdef __cplusplus
}
#endif
