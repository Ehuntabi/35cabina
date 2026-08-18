/* udp_rx.h - Receiver UDP de 35cabina.
 *
 * Configura el ESP32-S3 como STA, se asocia al SoftAP de la P4 y escucha
 * paquetes UDP broadcast en MINI_PROTO_UDP_PORT con el payload mini_msg_t.
 *
 * SSID/password reales en main/wifi_credentials.h (no versionado, ver
 * wifi_credentials.h.example y .gitignore).
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

void udp_rx_start(void);

#ifdef __cplusplus
}
#endif
