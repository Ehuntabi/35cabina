# 35cabina — satélite táctil de la P4 (autocaravana)

> **ES** | [**EN**](#en)

Proyecto **nuevo desde 0** para el módulo Guition JC3248W535 3.5"
(ESP32-S3 + AXS15231B QSPI + táctil, 320×480, LVGL 8.4.0), reconvertido en
**satélite de la P4** (`~/joint/victron`, pantalla de 7" del salón) en vez
de hacer su propio escaneo BLE/AES como el fork anterior
(`~/joint/victronsolardisplayesp-multi-device_pantalla_3.5`, descartado
como producto — su lógica Victron/BLE/AES/portal Wi-Fi no se reutiliza).

Se reutiliza:
- El **bring-up de hardware** ya estable del fork anterior (`esp_bsp.c/h`,
  `lv_port.c/h`, `display.h`) — pantalla QSPI, táctil, bus I2C compartido,
  LVGL. Estabilidad heredada de la saga del hang cerrada 18-may-2026 (mismo
  `sdkconfig`).
- La **arquitectura de red y datos** del satélite `victron_mini`
  (`~/joint/victron_mini`, ESP32-C6): recepción UDP broadcast del AP
  `VictronConfig` de la P4, protocolo `mini_msg_t` (32 bytes, versión 2,
  puerto 4242).

## Hardware

- **Pantalla**: Guition JC3248W535 3.5" — ESP32-S3, controlador AXS15231B
  (QSPI), táctil capacitivo, 320×480.
- **Acelerómetro** (inclinación al aparcar): ADXL345, I2C, colgado del bus
  ya compartido con el táctil (`bsp_i2c_get_bus_handle()`, GPIO4/GPIO8).

## Target / toolchain

**ESP-IDF v5.4.4** (obligatorio en todos los proyectos Victron de este
usuario, ver memoria `project_victron_esp_idf`). Target `esp32s3`.

## Estructura

```
35cabina/
├─ CMakeLists.txt
├─ partitions.csv          # factory ampliada (ya no hay particion spiffs)
├─ sdkconfig                # heredado y estable del fork 3.5" anterior
├─ components/
│  └─ config_storage/       # NVS: brillo, salvapantallas, rele (sin Victron)
└─ main/
   ├─ main.c                # bring-up + splash (Fase 0)
   ├─ esp_bsp.c/h            # pantalla QSPI + tactil + bus I2C
   ├─ lv_port.c/h, display.h # bring-up LVGL / panel
   ├─ ui/                    # ui_theme, ui_format (genericos, sin Victron)
   └─ net/                   # udp_rx + mini_proto (Fase 1)
```

## Hoja de ruta

Plan completo en `/home/db3/.claude/plans/polished-chasing-brooks.md` y en
la memoria de proyecto `project_pantalla_35_satelite_p4`. Resumen:

- **Fase 0** (este commit): scaffold + bring-up de hardware, splash en blanco.
- **Fase 1**: recepción UDP del broadcast de la P4 + pantalla de info
  agrupada (batería, batería motor, DC/DC, frigo+ventilador, aguas, exterior).
- **Fase 2**: carrusel de 3 pantallas por gesto (centro: info · derecha:
  repostajes/cambio de bombona · izquierda: inclinación).
- **Fase 3**: sensor de inclinación (ADXL345) sobre el bus I2C compartido.
- **Fase 4** (fuera de este repo, requiere luz verde aparte): canal de
  vuelta hacia la P4 (`~/joint/victron`) para que los repostajes/bombona
  lleguen al "viaje" (`trip_manager.c`).

## Build

```bash
. $HOME/esp/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
```

---

<a name="en"></a>
## EN

**New project from scratch** for the Guition JC3248W535 3.5" module
(ESP32-S3 + AXS15231B QSPI + touch, 320×480, LVGL 8.4.0), turned into a
**satellite of the "P4"** (the 7" salon display, `~/joint/victron`) instead
of doing its own BLE/AES scanning like the previous fork (discarded as a
product; its Victron/BLE/AES/Wi-Fi-portal logic is not reused here).

Reused:
- The already-stable **hardware bring-up** from the previous fork
  (`esp_bsp.c/h`, `lv_port.c/h`, `display.h`) — QSPI display, touch, shared
  I2C bus, LVGL. Stability inherited from the "hang saga" closed on
  2026-05-18 (same `sdkconfig`).
- The **network/data architecture** of the `victron_mini` satellite
  (ESP32-C6): UDP broadcast reception from the P4's `VictronConfig` AP,
  `mini_msg_t` protocol (32 bytes, version 2, port 4242).

See the roadmap above (Fases 0-4) for what's implemented vs. planned.

### Build

```bash
. $HOME/esp/esp-idf/export.sh
idf.py set-target esp32s3
idf.py build
```
