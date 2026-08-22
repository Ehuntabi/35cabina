# 35cabina — satélite táctil de la P4

Proyecto **nuevo desde 0** (no fork) para el módulo Guition JC3248W535 3.5"
(ESP32-S3 + AXS15231B QSPI + táctil, 320×480). Convierte ese hardware en un
satélite de la P4 (`~/joint/victron`), reutilizando el bring-up de hardware
del fork anterior (`~/joint/victronsolardisplayesp-multi-device_pantalla_3.5`,
descartado como producto) y la arquitectura de red del satélite
`victron_mini` (`~/joint/victron_mini`).

> Antes de cualquier trabajo de código no trivial aplicar
> [`andrej-karpathy-skills:karpathy-guidelines`](https://github.com/multica-ai/andrej-karpathy-skills):
> Think Before Coding · Simplicity First · Surgical Changes · Goal-Driven Execution.

- **Target**: `esp32s3`. **ESP-IDF**: v5.4.4 (obligatorio, ver memoria
  `project_victron_esp_idf`).
- **LVGL 8.4.0** — misma versión que `victron_mini` y que el fork anterior;
  permite portar patrones de UI (`view_quad.c`) sin migración de API.
- **Sin lógica Victron/BLE/AES/portal Wi-Fi propia** — todo eso lo hace la
  P4; este dispositivo solo recibe (`net/udp_rx.c`, protocolo
  `mini_proto.h`, MANTENER SINCRONIZADO con `~/joint/victron`) y, más
  el 22-ago-2026 **ya envía de vuelta** el inicio y el fin de viaje: `POST
  /api/viaje` contra el portal de la P4 (`main/net/p4_api.c`), que crea la
  carpeta del viaje en su SD. Los registros sueltos siguen sin enviarse —
  fase 3 del diseño, en `docs/superpowers/specs/`.
- **El satélite C6 (`victron_mini`) está DESCARTADO** (20-ago-2026): esta
  pantalla lo sustituye. Ya no hay que sincronizar `mini_proto.h` con él ni
  diseñar pensando en sus 32 bytes heredados. Ojo mientras siga enchufado:
  subir `MINI_PROTO_VERSION` lo deja mudo.
- Ver `README.md` para la hoja de ruta completa (Fases 0-4) y
  `/home/db3/.claude/plans/polished-chasing-brooks.md` para el plan de
  implementación detallado.
