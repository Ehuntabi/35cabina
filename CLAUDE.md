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

## `dependencies.lock` (versionado desde el 18-sep-2026)
- Se versiona con el mismo criterio que la P4: fija las versiones resueltas para
  que el build sea reproducible. Antes estaba en el `.gitignore`, así que este
  repo era el único de los dos sin cerrojo: si Espressif movía una versión de un
  componente gestionado, aquí se resolvía otra cosa sin que nadie se enterase.
- Cerrojo versionado en `.githooks/pre-commit`: aborta el commit si el
  `dependencies.lock` que va a entrar lleva una ruta absoluta (rompe el CI). En
  un clon nuevo hay que activarlo UNA vez: `git config core.hooksPath .githooks`.
- Chequeo rápido a mano: `grep -n "path: /" dependencies.lock` (sin resultados =
  limpio).

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
- `.github/workflows/build.yml` (16-sep-2026): compila con `idf.py build` en cada
  push/PR (docker `espressif/idf:v5.4.4`, target `esp32s3`). Pilla errores de
  COMPILACION, no sustituye probar en la placa. Desde el 3-oct-2026 ejecuta
  tambien `test/auditar.sh` (ver abajo).

## Auditoria (`test/auditar.sh`, 3-oct-2026)
- Equivalente al verificador de la P4, adaptado a lo que puede romperse AQUI:
  IDF v5.4.4 dentro del binario, LVGL 8.4.0 en el cerrojo, particion de coredump,
  Task WDT encendido y con panic, pilas de tarea >= 3072, copias de cadena sin
  limite, **LVGL desde tareas siempre bajo `lvgl_port_lock`**, los parametros del
  enlace (192.168.4.1 y la IP estatica .200 fuera del DHCP .2-.101 de la P4),
  `mini_proto.h` y el contrato completo con la P4, `CUERPO_MAX` dentro de
  `VIAJE_BODY_MAX`, `dependencies.lock` sin rutas absolutas y que el .bin no sea
  mas viejo que el ultimo commit.
- Se corre a mano con `bash test/auditar.sh` y solo en el CI. Las reglas que
  necesitan el repo de la P4 al lado (`mini_proto.h`, contrato, `CUERPO_MAX`) se
  omiten si no esta.
- **Lo que NO verifica**: nada de tiempo de ejecucion (Wi-Fi, pantalla, tactil):
  eso pide la placa.
