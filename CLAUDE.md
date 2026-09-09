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
- `.github/workflows/mini_proto_sync.yml` (único workflow de este repo por
  ahora, dos jobs):
  - `mini_proto_sync` (07-sep-2026): compara byte a byte
    `main/net/mini_proto.h` contra la copia de `Ehuntabi/victron-
    jc1060p470c-esp32p4` y falla si difieren.
  - `viaje_body_sync` (09-sep-2026, espejo del job del mismo nombre en
    victron): comprueba que `CUERPO_MAX` (`main/net/viaje_cola.h`, aquí)
    cabe con margen en `VIAJE_BODY_MAX` (`main/portal/config_server_viaje.c`,
    victron) — antes el guard solo vivía en el CI de victron, así que este
    repo, el único que puede romper el límite subiendo `CUERPO_MAX`, no
    tenía forma de detectarlo él mismo.
  - No hay build de ESP-IDF en CI (a diferencia de victron, que sí lo tiene).
