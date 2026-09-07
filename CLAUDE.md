# 35cabina — satélite táctil de la P4

Proyecto **nuevo desde 0** (no fork) para el módulo Guition JC3248W535 3.5"
(ESP32-S3 + AXS15231B QSPI + táctil, 320×480). Convierte ese hardware en un
satélite de la P4 (`~/joint/victron`), reutilizando el bring-up de hardware
del fork anterior (`~/joint/victronsolardisplayesp-multi-device_pantalla_3.5`,
descartado como producto).

- **Target**: `esp32s3`. **ESP-IDF**: v5.4.4 (obligatorio, ver memoria
  `project_victron_esp_idf`).
- **LVGL 8.4.0** — misma versión que el fork anterior; permite portar
  patrones de UI (`view_quad.c`) sin migración de API.
- **Sin lógica Victron/BLE/AES/portal Wi-Fi propia** — todo eso lo hace la
  P4; este dispositivo solo **recibe** telemetría (`net/udp_rx.c`, protocolo
  `mini_proto.h`, MANTENER SINCRONIZADO con `~/joint/victron`).
  Desde el 22-ago-2026 **ya envía de vuelta** el inicio y el fin de viaje, y
  desde el 23-ago-2026 también cada registro suelto (parada, aguas,
  repostaje, peaje, bombona, avería/mant.): `POST /api/viaje` contra el
  portal de la P4 (`main/net/p4_api.c`), encolado en NVS si la P4 no
  responde (`main/net/viaje_cola.c`).
- Ver `README.md` para la hoja de ruta completa (Fases 0-4) y
  `docs/superpowers/specs/` para los diseños detallados de cada fase.

## CI
- Job `mini_proto_sync` (07-sep-2026, único CI de este repo por ahora): compara
  byte a byte `main/net/mini_proto.h` contra la copia de `Ehuntabi/victron-
  jc1060p470c-esp32p4` y falla si difieren. No hay build de ESP-IDF en CI
  (a diferencia de victron, que sí lo tiene).
