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

> **El C6 queda descartado (20-ago-2026): esta pantalla lo sustituye.**
> `mini_proto.h` ya sólo se sincroniza con `~/joint/victron`, y el protocolo
> puede crecer sin arrastrar los 32 bytes heredados. Aviso mientras el C6
> siga enchufado: subir `MINI_PROTO_VERSION` lo deja mudo, porque rechaza
> las versiones que no conoce.

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
  en un **bus I2C propio** (`I2C_NUM_1`, `IO17`=SDA / `IO18`=SCL, los de
  los **JST de 4 pines**) — **no** el bus interno del táctil (GPIO4/GPIO8):
  el esquemático muestra que ese bus es cableado interno pantalla+táctil
  sin pad accesible desde fuera, así que no había forma física de
  colgarse de él.

  Se usan los JST de 4 pines y no el "Extended IO" de 8 porque **aquéllos
  traen `3V3` y `GND` en el mismo conector**: el sensor cuelga de un solo
  cable de 4 hilos en vez de repartirse entre dos conectores. `IO5`/`IO6`
  (y `IO7`/`IO9`/`IO14`/`IO15`/`IO16`) servirían igual — en el S3 el I2C va
  por matriz GPIO — pero obligan a llevar la alimentación aparte.

  **Cableado del módulo ADXL345**: `SDA`→IO17, `SCL`→IO18, `VCC`→3V3,
  `GND`→GND, y además **`CS`→3V3** (si queda al aire el chip arranca en modo
  SPI y no responde por I2C) y **`SDO`→GND** (es lo que fija la dirección
  `0x53`; a 3V3 sería `0x1D` y el firmware no lo encontraría). `INT1`/`INT2`
  sin conectar. No hacen falta pull-ups externos: `tilt.c` activa los
  internos. **El orden físico de pines de los JST no está verificado** —
  comprobarlo con el esquemático oficial o un polímetro antes de enchufar.

  Sin verificar aún en placa real el mapeo de ejes pitch/roll según cómo
  quede montado el sensor (ver comentario en `tilt.c`).

## Target / toolchain

**ESP-IDF v5.4.4** (obligatorio en todos los proyectos Victron de este
usuario, ver memoria `project_victron_esp_idf`). Target `esp32s3`.

## Estructura

```
35cabina/
├─ empiezo / termino       # sync multi-equipo (PC/portatil) via git
├─ CMakeLists.txt
├─ partitions.csv          # factory ampliada (ya no hay particion spiffs)
├─ sdkconfig                # heredado y estable del fork 3.5" anterior
├─ components/
│  └─ config_storage/       # NVS: brillo, salvapantallas, rele (sin Victron)
└─ main/
   ├─ main.c                          # bring-up + arranque UI/red
   ├─ data_model.c/h                  # snapshot de mini_msg_t protegido por lock
   ├─ tilt.c/h                        # ADXL345 (bus I2C propio IO17/IO18) -- pitch/roll (Fase 3)
   ├─ esp_bsp.c/h                     # pantalla QSPI + tactil + bus I2C
   ├─ lv_port.c/h, display.h          # bring-up LVGL / panel
   ├─ wifi_credentials.h.example      # plantilla; el real NO se versiona
   ├─ ui/
   │  ├─ ui_theme.c/h, ui_format.c/h  # genericos, sin Victron
   │  ├─ nav.c/h                      # carrusel de 3 pantallas por gesto (Fase 2)
   │  ├─ view_info.c/h                # centro: info agrupada (Fase 1)
   │  ├─ view_registro.c/h            # derecha: menu de 5 registros (Fase 2)
   │  ├─ view_inclinacion.c/h         # izquierda: burbuja de nivel (Fase 3)
   │  └─ view_ajustes.c/h             # SSID/password de la P4, editable sin reflashear
   └─ net/
      ├─ mini_proto.h                 # protocolo compartido con la P4 (el mini C6 ya no cuenta)
      └─ udp_rx.c/h                   # STA + receptor UDP :4242 (Fase 1)
```

## Hoja de ruta

Plan completo en `/home/db3/.claude/plans/polished-chasing-brooks.md` y en
la memoria de proyecto `project_pantalla_35_satelite_p4`. Resumen:

- **Fase 0** (hecho): scaffold + bring-up de hardware + splash de arranque
  con el logo del fork anterior (2s sobre el top layer, luego se revela
  el carrusel).
- **Fase 1** (hecho): recepción UDP del broadcast de la P4 + pantalla de
  info agrupada en grid (batería, batería motor, DC/DC, frigo+ventilador,
  aguas, exterior). El SSID/password se guardan en **NVS**, editables sin
  reflashear desde el icono de engranaje (⚙) arriba-derecha de la
  pantalla de info → **Ajustes Wi-Fi** (`view_ajustes.c`) — pensado para
  cambiar de P4 (ej. la de repuesto para pruebas) sin ordenador de por
  medio. `main/wifi_credentials.h` (ignorado por git, copiar desde
  `wifi_credentials.h.example`) solo se usa como valor de fábrica la
  primera vez que arranca con NVS vacía.
- **Fase 2** (hecho): carrusel de 3 pantallas por gesto horizontal (centro:
  info · derecha: menú de 5 iconos — **Viaje** (iniciar/finalizar),
  **Repostaje** (importe+moneda, litros, precio/litro calculado),
  **Peaje** (importe+moneda), **Bombona** (cuántas: 1 o 2, precio total),
  **Mantenimiento** (tipo: aceite/filtro de aceite/correa/ruedas + km)
  · izquierda: placeholder de inclinación).

  El **menú de iconos ocupa toda la pantalla**: reparto 3+2 (tres celdas de
  146×145 arriba, dos de 225×145 abajo), cada categoría con su **color de
  fondo** propio. Los tamaños van en píxeles y no en porcentaje **a
  propósito**: en LVGL el `pad_gap` no se descuenta del porcentaje y las tres
  celdas de arriba se salían de fila. Por lo justo del encaje (458 de 460 px
  útiles), `view_registro_create()` anula el padding y el borde que el tema de
  LVGL pone en la pantalla; si no, la tercera celda bajaría de fila.

  **Fondos claros con el contenido en negro**, no al revés: la primera versión
  usaba la familia Material 800 con texto blanco y daba 3,8-6,4:1 de contraste,
  que en la placa se veía apagado. Invertido pasa de 8:1 en las cinco. Los
  valores exactos y el cálculo están en `view_registro.c`.

  **Ningún formulario pide coordenada GPS ni hora** (se quitaron el
  20-ago-2026). Tecleadas a mano no aportan y estorban en el surtidor; cuando
  se abra la Fase 4 la fecha y la hora las pone **la P4 al recibir el evento**,
  que es quien tiene el reloj bueno. Ojo: **no hay GPS en ningún aparato del
  sistema** — ni en esta pantalla ni en la P4 — así que hoy la posición no
  puede registrarla nadie; haría falta un módulo GPS de verdad.

  Selector de moneda en los campos de importe (EUR por defecto, contempla
  otras monedas europeas — GBP/CHF/SEK/NOK/DKK/PLN/CZK/HUF/RON), **solo con
  código ASCII**: las fuentes Montserrat de LVGL se compilan con el rango
  `0x20-0x7F,0xB0,0x2022`, así que el símbolo del euro salía como un cuadrado
  vacío. Misma regla para toda la interfaz: nada de acentos ni eñes en los
  textos que se pintan.

  **Al tocar un campo se abre un editor a pantalla completa**
  (`ui/entry_screen.c`): el valor en letra 40 arriba y el teclado ocupando
  240 de los 320 px, en vez del teclado a media pantalla que además tapaba el
  campo que estabas escribiendo.
  Ningún botón envía nada todavía — eso es la Fase 4; "Iniciar/Finalizar
  viaje" es el primer candidato cuando se abra (pedido explícito del
  usuario: que el 3.5" mande el comando de viaje, no la P4).
- **Fase 3** (hecho; sensor ya detectado en placa real el 20-ago-2026, falta
  validar la lectura): burbuja de nivel clásica (círculo que se desplaza, sin
  assets de imagen) leyendo el ADXL345 por I2C a 5Hz. Botón "Calibrar nivel"
  en la propia pantalla: promedia ~20 lecturas y guarda el offset en NVS
  (`config_storage.c`, namespace `"tilt"`). Si el sensor no responde al
  arrancar, la pantalla lo indica ("Sensor ADXL345 no detectado") en vez de
  fallar.

  Estado real: con el ADXL345 cableado a IO17/IO18 el arranque ya dice
  `tilt: ADXL345 OK`. **Pendiente**: calibrar con la autocaravana nivelada y
  comprobar que pitch/roll no salen cruzados según la orientación de montaje
  (ver `tilt.c`).
- **Fase 4** (fuera de este repo, requiere luz verde aparte): canal de
  vuelta hacia la P4 (`~/joint/victron`) para que los repostajes/bombona
  lleguen al "viaje" (`trip_manager.c`).

## Trabajo multi-equipo (`./empiezo` / `./termino`)

Para trabajar indistintamente en PC y portátil manteniendo el repo
siempre sincronizado (mismo patrón que `~/joint/victron` y
`~/joint/victron_mini`):

- **`./empiezo`** — al sentarte: aparta cambios locales (stash), `git
  pull --rebase`, recupera tus cambios y abre Claude Code. `--no-claude`
  solo sincroniza.
- **`./termino`** — al acabar: `git add -A`, commit (Claude redacta el
  mensaje, o pasa el tuyo: `./termino "mensaje"`), `git push`, y verifica
  que no queda nada al aire antes de cambiar de equipo.

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

> **The C6 is retired (2026-08-20): this display replaces it.**
> `mini_proto.h` is now kept in sync with `~/joint/victron` only, and the
> protocol is free to grow beyond the inherited 32 bytes. Caveat while the
> C6 is still plugged in: bumping `MINI_PROTO_VERSION` silences it, since it
> rejects versions it doesn't know.

See the roadmap above (Fases 0-4) for what's implemented vs. planned.

### Build

```bash
. $HOME/.espressif/esp-idf-5.4/export.sh
cp main/wifi_credentials.h.example main/wifi_credentials.h   # fill in real values
idf.py set-target esp32s3
idf.py build
```
