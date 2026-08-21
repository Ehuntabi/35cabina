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
  `GND`→GND. `CS`, `SDO`, `INT1` e `INT2` **sin conectar**, que es lo que
  hacen todos los ejemplos: en las placas tipo **GY-291** el `CS` ya lleva un
  pull-up de 10k a VCC (4k7 en algunas) y el `SDO`/`ADDR` un pull-down de
  4k7, así que el chip arranca solo en modo I2C y en la dirección `0x53`, que
  es la que usa `tilt.c`. Esa placa trae además sus propios pull-ups de 4k7
  en SDA/SCL; los internos que activa `tilt.c` son solo una red de seguridad.

  Solo hay que cablearlos con el **chip pelado** o con un módulo que no
  traiga esas resistencias: entonces `CS`→3V3 es **obligatorio** (al aire no
  hay modo por defecto: no responde ni por I2C ni por SPI) y `SDO`→GND fija
  la dirección `0x53` — a 3V3 sería `0x1D` y el firmware no lo encontraría.

  **El orden físico de pines de los JST no está verificado** — comprobarlo
  con el esquemático oficial o un polímetro antes de enchufar.

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
  info agrupada en grid: batería, batería motor, frigo+ventilador, aguas y
  exterior, repartidas **3 arriba + 2 abajo** sobre una rejilla de seis
  columnas (las de abajo ocupan tres cada una). **La tarjeta de DC/DC se
  quitó** el 20-ago-2026 a petición del usuario; el dato sigue llegando en
  el `mini_msg_t`, simplemente no se pinta. Al quitarla hubo que borrar
  también su refresco: se quedaba apuntando a punteros nulos y habría
  colgado la pantalla en cuanto llegara el primer paquete.

  El SSID/password se guardan en **NVS**, editables sin reflashear desde la
  **tarjeta de Wi-Fi del menú de registros** → `view_ajustes.c`; pensado
  para cambiar de P4 (ej. la de repuesto para pruebas) sin ordenador de por
  medio. Antes era un engranaje (⚙) en la esquina de esta pantalla, que se
  retiró para no tener dos puertas a lo mismo.
  `main/wifi_credentials.h` (ignorado por git, copiar desde
  `wifi_credentials.h.example`) solo se usa como valor de fábrica la
  primera vez que arranca con NVS vacía.
- **Fase 2** (hecho): carrusel de 3 pantallas por gesto horizontal (centro:
  info · derecha: menú de 5 iconos — **Viaje** (iniciar/finalizar),
  **Repostaje** (moneda arriba; importe y litros en la misma línea;
  precio/litro calculado), **Peaje** (moneda arriba, importe debajo en
  letra 40), **Bombona** (cuántas: 1 o 2, precio total),
  **Mantenimiento** (ver abajo) · izquierda: placeholder de inclinación).
  Una sexta celda, **Wi-Fi**, no es un registro: salta a `view_ajustes.c`.

  El **menú ocupa toda la pantalla**: **3+3, seis celdas de 146×145**, cada
  una con su **color de fondo**. Los tamaños van en píxeles y no en
  porcentaje **a propósito**: en LVGL el `pad_gap` no se descuenta del
  porcentaje y las tres celdas de arriba se salían de fila. Por lo justo del
  encaje (458 de 460 px útiles), `view_registro_create()` anula el padding y
  el borde que el tema de LVGL pone en la pantalla; si no, la tercera celda
  bajaría de fila.

  **Mantenimiento** usa **casillas, no un desplegable**: con el mismo
  kilometraje puedes haber hecho varias cosas (el aceite Y su filtro es el
  caso típico). Seis opciones — aceite, filtro de aceite, filtro de aire,
  filtro de habitáculo, correa y ruedas — en dos columnas. Al marcar
  **Ruedas** aparece un contador de cuántas (1-4), oculto el resto del
  tiempo. Debajo, **Km y Coste comparten línea**. Aviso de espacio: con
  Ruedas marcado el formulario queda al límite de los 320 px y puede pedir
  un pequeño desplazamiento para llegar al botón de guardar.

  **Toda acción pide confirmación** (`ui/confirm_screen.c`): al pulsar
  Guardar aparece una pantalla con **lo que se ha introducido** y un
  "¿Es correcto?" con dos botones grandes. Viaje también pregunta, sin
  resumen — ahí importa más que en ningún sitio, porque finalizar un viaje
  por un roce cierra el registro en curso de la P4. Los campos vacíos salen
  como `--`, para que se vea que faltan antes de aceptar.

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

  **Salir de la página con un gesto vuelve al menú de iconos**
  (`view_registro_reset()`, la llama `nav.c`). Antes el formulario seguía
  abierto por detrás y al regresar te lo encontrabas tal cual, en vez del
  menú. Se cierran también el editor de campo y la confirmación.

  **Cada formulario se abre siempre en blanco** (`clear_forms()`, la llama
  `show_grid()`): al volver al menú —por el botón Volver, por haber guardado
  o por un gesto— se vacían campos, casillas y contadores, y la moneda vuelve
  a `EUR`. Antes los datos se quedaban dentro y al reabrir la categoría
  seguía ahí el importe anterior; el riesgo no era teclear de más, era
  **guardar sin mirar el dato de la vez pasada**. Fuera de la zona euro
  obliga a elegir la moneda en cada apunte, que es el precio de no anotar
  euros con la moneda del país anterior aún puesta.

  Y **un deslizamiento no cuenta como toque** (`lv_indev_wait_release()` en
  `nav.c`): LVGL manda el `CLICKED` al objeto donde se apoyó el dedo aunque
  por el medio haya saltado un gesto, así que al deslizar desde encima de un
  campo se cambiaba de pantalla y acto seguido el clic reabría el formulario
  por detrás — al volver te lo encontrabas abierto, de forma intermitente
  (solo si el dedo arrancaba encima de un widget).
  Ningún botón envía nada todavía — eso es la Fase 4; "Iniciar/Finalizar
  viaje" es el primer candidato cuando se abra (pedido explícito del
  usuario: que el 3.5" mande el comando de viaje, no la P4).
- **Fase 3** (hecho y validado en placa real el 21-ago-2026): burbuja de nivel
  clásica (círculo que se desplaza, sin
  assets de imagen) leyendo el ADXL345 por I2C a 5Hz. Botón "Calibrar nivel"
  en la propia pantalla: promedia ~20 lecturas y guarda el offset en NVS
  (`config_storage.c`, namespace `"tilt"`). Si el sensor no responde al
  arrancar, la pantalla lo indica ("Sensor ADXL345 no detectado") en vez de
  fallar.

  Estado real: con el ADXL345 cableado a IO17/IO18 el arranque dice
  `tilt: ADXL345 OK` y **el mapeo de ejes está comprobado** — cabeceo y
  balanceo salen en el sentido correcto con el sensor montado como está, así
  que no hay que intercambiar `ax`/`ay` en `tilt.c`. **Pendiente**: calibrar
  con la autocaravana nivelada (botón "Calibrar nivel", guarda el offset en
  NVS y solo hay que hacerlo una vez).
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
