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

- **Pantalla**: Guition JC3248W535 3.5" — ESP32-S3-WROOM-1, controlador
  AXS15231B (QSPI), táctil capacitivo, panel nativo 320×480 pero
  **resolución lógica LANDSCAPE 480×320** una vez aplicada la rotación de
  90° (`esp_bsp.c:390-396` intercambia hres/vres; verificado contra el
  código el 18-ago-2026, un comentario viejo en `ui_theme.h` decía
  "portrait" y estaba mal). 8MB PSRAM, 16MB flash.
- **Ranura TF/microSD**: el módulo la trae físicamente ("Reserve the TF
  card interface", datasheet del fabricante), pero **el software actual
  no la usa ni la configura** — ni este proyecto ni el fork anterior
  montan `sdmmc`/`sdspi`. Si algún día hace falta almacenamiento local en
  la propia 35cabina, está disponible sin cableado adicional.
- **Conector de expansión "Extended IO"** (JST1.25 8 pines, ver esquemático
  oficial): expone `IO5, IO6, IO7, IO15, IO16, IO46, IO9, IO14` libres —
  ninguno usado por la pantalla/táctil/TF card de este módulo. Hay además
  dos JST de 4 pines más pequeños con `IO17`/`IO18` + alimentación.
- **Acelerómetro** (inclinación al aparcar): ADXL345, I2C address `0x53`,
  en un **bus I2C propio** (`I2C_NUM_1`, `IO5`=SDA / `IO6`=SCL del
  conector Extended IO) — **no** el bus interno del táctil (GPIO4/GPIO8):
  el esquemático muestra que ese bus es cableado interno pantalla+táctil
  sin pad accesible desde fuera, así que no había forma física de
  colgarse de él. Sin verificar aún en placa real el mapeo de ejes
  pitch/roll según cómo quede montado el sensor (ver comentario en
  `tilt.c`).

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
   ├─ main.c                          # bring-up + arranque UI/red
   ├─ data_model.c/h                  # snapshot de mini_msg_t protegido por lock
   ├─ tilt.c/h                        # ADXL345 (bus I2C propio IO5/IO6) -- pitch/roll (Fase 3)
   ├─ esp_bsp.c/h                     # pantalla QSPI + tactil + bus I2C
   ├─ lv_port.c/h, display.h          # bring-up LVGL / panel
   ├─ wifi_credentials.h.example      # plantilla; el real NO se versiona
   ├─ ui/
   │  ├─ ui_theme.c/h, ui_format.c/h  # genericos, sin Victron
   │  ├─ nav.c/h                      # carrusel de 3 pantallas por gesto (Fase 2)
   │  ├─ view_info.c/h                # centro: info agrupada (Fase 1)
   │  ├─ view_repostaje.c/h           # derecha: repostaje + bombona (Fase 2)
   │  └─ view_inclinacion.c/h         # izquierda: burbuja de nivel (Fase 3)
   └─ net/
      ├─ mini_proto.h                 # protocolo compartido con la P4 y el mini
      └─ udp_rx.c/h                   # STA + receptor UDP :4242 (Fase 1)
```

## Hoja de ruta

Plan completo en `/home/db3/.claude/plans/polished-chasing-brooks.md` y en
la memoria de proyecto `project_pantalla_35_satelite_p4`. Resumen:

- **Fase 0** (hecho): scaffold + bring-up de hardware.
- **Fase 1** (hecho): recepción UDP del broadcast de la P4 + pantalla de
  info agrupada en grid (batería, batería motor, DC/DC, frigo+ventilador,
  aguas, exterior). El SSID/password reales viven en
  `main/wifi_credentials.h`, ignorado por git (repo público) — copiar desde
  `wifi_credentials.h.example` y rellenar con los valores reales antes de
  compilar.
- **Fase 2** (hecho): carrusel de 3 pantallas por gesto horizontal (centro:
  info · derecha: repostajes/cambio de bombona con teclado en pantalla ·
  izquierda: placeholder de inclinación). El botón "Guardar" de los
  formularios todavía no envía nada — eso es la Fase 4.
- **Fase 3** (hecho, sin probar en placa real): burbuja de nivel clásica
  (círculo que se desplaza, sin assets de imagen) leyendo el ADXL345 por
  I2C a 5Hz. Botón "Calibrar nivel" en la propia pantalla: promedia ~20
  lecturas y guarda el offset en NVS (`config_storage.c`, namespace
  `"tilt"`). Si el sensor no responde al arrancar, la pantalla lo indica
  ("Sensor ADXL345 no detectado") en vez de fallar.
- **Fase 4** (fuera de este repo, requiere luz verde aparte): canal de
  vuelta hacia la P4 (`~/joint/victron`) para que los repostajes/bombona
  lleguen al "viaje" (`trip_manager.c`).

## Build

```bash
. $HOME/.espressif/esp-idf-5.4/export.sh   # alias "get_idf" en ~/.bashrc
cp main/wifi_credentials.h.example main/wifi_credentials.h   # rellenar valores reales
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
. $HOME/.espressif/esp-idf-5.4/export.sh
cp main/wifi_credentials.h.example main/wifi_credentials.h   # fill in real values
idf.py set-target esp32s3
idf.py build
```
