# 35cabina — satélite táctil de la P4 (autocaravana)

> **ES** | [**EN**](#en)

> 📖 **[Manual de uso](docs/MANUAL.md)** ([PDF](docs/MANUAL.pdf)) — cómo se usan
> viaje, paradas, repostajes, peajes, bombonas y mantenimiento, en lenguaje llano.
> El PDF se regenera con `python3 docs/manual_a_pdf.py docs/MANUAL.md docs/MANUAL.pdf`.

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
  de la P4 (**`ESP_DC078D`**, no `VictronConfig`: ese es sólo el valor de
  fábrica del código de la P4, y lo que manda es lo guardado en su NVS —
  comprobado en su log el 21-ago-2026), protocolo `mini_msg_t` (36 bytes,
  versión 3, puerto 4242).

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

  Mapeo de ejes pitch/roll **verificado en placa real el 21-ago-2026** con el
  sensor montado como está: cabeceo y balanceo salen en el sentido correcto.
  Si algún día se cambia su orientación física hay que volver a comprobarlo
  (ver comentario en `tilt.c`).

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
   ├─ salida.c/h                      # la salida en curso y lo que quedo abierto (NVS)
   ├─ wifi_credentials.h.example      # plantilla; el real NO se versiona
   ├─ ui/
   │  ├─ ui_theme.c/h, ui_format.c/h  # genericos, sin Victron
   │  ├─ nav.c/h                      # carrusel de 3 pantallas por gesto (Fase 2)
   │  ├─ view_info.c/h                # centro: info agrupada (Fase 1)
   │  ├─ view_registro.c/h            # derecha: los 7 menus de la salida + formularios (Fase 2)
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

  **Versión 3 del protocolo (21-ago-2026)**: se añadió `epoch_local` — el
  reloj de la P4, en segundos desde 1970 **ya desplazados a su hora local**, 0
  mientras su RTC no tenga hora buena. Esta pantalla **no tiene reloj**: no
  lleva RTC ni pila y se apaga con el contacto, así que al encender no sabe ni
  qué día es. Con ese campo cuenta lo que dura una parada abierta (ver
  Fase 2), y le hacen falta las dos cosas: el **día** para las noches
  (`epoch_local / 86400`) y la **hora** para las áreas que cobran por periodos
  de 24 h desde que entras.

  Desplazado a local y no en UTC **a propósito**: así el satélite saca el día
  con una división entera y no necesita saber nada de husos ni de horario de
  verano. En la P4 se arma a mano desde los campos de `localtime_r()` con
  `days_from_civil`, porque su newlib no trae `tm_gmtoff` ni `timegm()` — y a
  mano sale exacto, sin el "adivina el horario de verano" que haría `mktime()`
  en las dos horas ambiguas del año.

  El mensaje pasó de 32 a 36 bytes; **subir la versión obliga a reflashear los
  dos aparatos**.

  El SSID/password se guardan en **NVS**, editables sin reflashear desde la
  **tarjeta de Wi-Fi del menú de registros** → `view_ajustes.c`; pensado
  para cambiar de P4 (ej. la de repuesto para pruebas) sin ordenador de por
  medio. Antes era un engranaje (⚙) en la esquina de esta pantalla, que se
  retiró para no tener dos puertas a lo mismo.

  **"Guardar y reconectar" pide confirmación si has cambiado algo** (y solo
  entonces: si no has tocado el SSID ni la contraseña, avisa con un "No has
  cambiado nada" y no hace nada). Equivocarse aquí deja la pantalla sin P4 y
  sin datos, y para volver atrás hay que teclear a mano lo de antes con el
  vehículo en marcha. El diálogo es el mismo `confirm_screen.c` de los
  registros: **se muda a la pantalla activa** en cada apertura, porque nace
  colgado de la de registros y Ajustes vive en otra pantalla del carrusel.
  `main/wifi_credentials.h` (ignorado por git, copiar desde
  `wifi_credentials.h.example`) solo se usa como valor de fábrica la
  primera vez que arranca con NVS vacía.
- **Fase 2** (hecho): carrusel de 3 pantallas por gesto horizontal (centro:
  info · derecha: el cuaderno de registros · izquierda: inclinación).

  **El cuaderno se organiza alrededor de la SALIDA** y no de las categorías
  (rediseño del 23-ago-2026, `docs/superpowers/specs/2026-08-23-pantalla-registros-salidas-design.md`).
  La pantalla se apaga al quitar el contacto, y todo se apoya en eso:
  **declaras al llegar, rellenas al salir**.

  **Siete pantallas de menú**, una detrás de otra y **un botón grande en cada
  una** (`crear_menus()` en `view_registro.c`): principal → tipo de salida
  (viaje / puntual) → menú de salida → las seis cosas de un viaje → por qué
  paras → dónde duermes; más la de las cuatro de una salida puntual.
  Configuración queda pequeño y gris en todas. Arriba, una franja de 26 px con
  la hora y dos puntos (GPS y P4) que **caducan con el enlace**.

  Los tamaños van en píxeles y no en porcentaje **a propósito**: en LVGL el
  `pad_gap` no se descuenta del porcentaje y las celdas de la derecha se salen
  de fila. Con la franja arriba, el cuerpo útil es 456×270 → celdas de 145×130
  en rejilla de tres, 223 de ancho en la de dos. `view_registro_create()` anula
  el padding y el borde que el tema de LVGL pone en la pantalla.

  **Estado de la salida en NVS** (`main/salida.{c,h}`): la salida en curso y
  hasta **4 eventos abiertos a la vez**, en un único blob de una escritura.
  Cuatro porque en una estancia larga conviven parada, repostaje, bombona y
  peaje. Marca de vida cada 10 min para saber a qué hora se fue la luz.

  ⏳ **Lo que falta del rediseño**: las pantallas de **al arrancar** — rellenar
  lo que quedó abierto, prolongar/finalizar parada y el aviso de «estuviste
  parado desde las 19:40». Hoy se declara y el apunte se queda abierto (la tira
  del menú dice cuántos). El **peaje** es el único que se guarda entero, porque
  se rellena en el momento.

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

  **La pantalla de Viaje es contextual** (`viaje_refresh()`), porque
  "Finalizar viaje" no tiene sentido si no has iniciado ninguno:

  - **Sin viaje**: el mensaje y un único botón grande, **Iniciar viaje**.
  - **Con viaje en marcha**: el título pasa a `VIAJE EN CURSO`, el botón
    grande es **Anotar parada** y **Finalizar viaje** queda **pequeño y
    abajo** — la acción habitual se lleva la pantalla y la destructiva no se
    toca de un roce. La celda del menú pasa a decir "Viaje en curso".
  - **Con una parada abierta** aparece además **Finalizar parada**, pequeño y
    en azul, encima del rojo: cerrar una parada es rutina y terminar el viaje
    no, así que se distinguen por color y el destructivo queda el último. Sale
    **haya viaje o no**: si el viaje se terminó con una parada sin cerrar,
    sigue habiendo que cerrarla y éste es el único sitio desde donde hacerlo.

  El estado lo lleva **la propia pantalla** y se guarda en NVS (namespace
  `viaje`, `load_trip_active()`/`save_trip_active()`), así que un corte de
  corriente en mitad de un viaje no devuelve el menú a "sin viaje". Ojo: la
  P4 sigue siendo la dueña del viaje de verdad; esto es solo **lo que cree la
  35cabina** hasta que la Fase 4 abra el canal de vuelta y pueda
  preguntárselo. Si las dos se descoordinan, hoy no se enteran.

  **Parada** (`build_parada`) cuelga de Viaje y su Volver regresa allí, no al
  menú. Cinco casillas de dos clases distintas:

  - **Lo que haces** — Vaciado · Llenado · Agua potable. Se marcan libremente
    y a la vez: en un área sueles vaciar Y llenar en la misma parada. **Agua
    potable va aparte de Llenado** porque se puede parar solo por eso: una
    fuente al borde de la carretera no es ni un vaciado ni un área.
  - **Dónde has parado** — Pernocta gratis · Área · Camping. Los tres son
    **excluyentes**: al marcar uno se **desmarcan solos** los otros dos. Se
    hace desmarcando y no poniéndolos en gris para que cambiar de idea sea un
    único toque sobre el que quieres, sin acordarse de quitar el anterior.

  **Área y Camping son las paradas que se pagan**, así que solo ellas sacan el
  **precio** (con moneda) y el botón de **Servicios**; una pernocta gratis no
  tiene ni lo uno ni lo otro. Aparecen y desaparecen igual que el contador de
  ruedas del mantenimiento.

  El precio es **por unidad de estancia**, no el total: es lo que se anuncia a
  la entrada y lo único que permite calcular la cuenta cuando la parada acaba
  días después. Y la unidad no siempre es la noche — **hay áreas que cobran
  por periodos de 24 h desde que entras** —, así que junto al precio hay un
  interruptor **`Noche` / `24 h`**, en la misma línea que el rótulo:

  - **Camping**: siempre por noches. El interruptor se esconde y el rótulo lo
    dice entero, "Precio por noche".
  - **Área**: sale el interruptor y lo dice el botón marcado.

  **Importe, moneda y selector van los tres en la misma línea y grandes**
  (importe en letra 32, los otros dos en 20): son el mismo dato —"cuánto
  cuesta cada noche / cada 24 h"— y leerlo de un vistazo importa más que su
  tamaño por separado. El reparto es **elástico** (4/2/3), no en píxeles: en
  un camping, al esconderse el selector, importe y moneda se reparten su hueco
  solos en vez de dejar un agujero.

  Por eso la fila de precio de la parada no usa `make_money_field()` sino la
  suya, `make_parada_precio_row()`: aquélla deja importe y moneda en letra 24
  sin sitio para nada más. Alturas: rótulo 16 + fila 52 + huecos = 74, que con
  cabecera 48 + casillas 116 + acciones 50 + 12 de huecos suman 300 de los 304
  útiles.

  **Al cambiar de sitio se borran precio, cobro y servicios**, que eran del
  anterior — el mismo problema que tenía el peaje guardándose el importe de la
  vez pasada.

  **Una parada con sitio queda ABIERTA** (`parada_abrir_si_procede()`): no
  termina cuando la guardas, sino cuando te vas, que puede ser días después y
  con la pantalla apagada por medio (se va con el contacto). Se guarda en NVS
  (namespace `parada`) el sitio, el precio, la moneda y el día de llegada. Las
  paradas de solo vaciado, llenado o agua se acaban en el sitio y no dejan
  nada abierto.

  **Al volver a encender**, en cuanto la P4 dice qué día es, sale el aviso
  sobre la pantalla principal:

  ```
          Fin de la parada?
          Camping · 3 noches
          Total:  75.00 EUR
     [ No ]          [ Si, terminar ]
  ```

  Ese aviso sale **una sola vez por encendido**. Contestar **No** deja la
  parada abierta y no vuelve a preguntar solo: para cerrarla cuando tú quieras
  está el botón **Finalizar parada** de la pantalla de Viaje.

  La cuenta depende del cobro: **por noche** son cambios de día de calendario;
  **por 24 h** son periodos desde que entras, **redondeando hacia arriba** (25
  horas son 2), que es como cobran ellos — más vale que la cuenta salga alta y
  no baja. **Mínimo 1** en los dos casos: si llegas y te vas el mismo día la
  parada ha existido igual, y se paga igual. Total = precio × unidades; sin
  precio (pernocta gratis) no sale esa línea. Contestar **No** deja la parada
  abierta y **se vuelve a preguntar en el siguiente arranque**, que es justo lo
  que quieres si sigues allí.

  **El reloj sale de la P4 y no de aquí**: la 3.5" no tiene RTC ni pila, se
  apaga con el contacto y al encender no sabe ni qué día es. Por eso el
  protocolo lleva `epoch_local` desde la versión 3 (ver arriba). **Sin ese dato
  no se abre parada**: más vale no contar que inventarse las noches.

  Pero **eso se dice, no se calla** (`confirm_screen_aviso()`, un cartel de una
  sola salida sin el botón de "No, corregir"). Antes se guardaba la parada, la
  pantalla decía "guardado" y por dentro no apuntaba nada, así que te ibas
  creyendo que se estaba contando la estancia. Lo mismo al pulsar *Finalizar
  parada* sin P4: dice por qué no puede en vez de no hacer nada, que parecería
  que el botón está roto.

  Si la P4 está apagada o fuera de alcance, la pregunta del arranque espera a
  que aparezca. Síntoma en el log cuando no la encuentra:
  `udp_rx: Desconectado reason=201` (`NO_AP_FOUND`) cada pocos segundos.

  Los **servicios** (Agua potable · Vaciado grises · Vaciado WC ·
  Electricidad · Duchas/WC · Basura) más una séptima casilla, **Valoración**,
  que no marca nada: abre su propia pantalla (ver abajo). Al volver, esa
  casilla muestra **la nota elegida** en vez de la palabra "Valoración"
  ("Valoración: Recomendado" son 23 caracteres y en media rejilla entran ~16).
  Los servicios son el mismo dato con dos lecturas, así
  que comparten lista y pantalla y solo cambia el rótulo que la explica: en un
  área es *"Lo que ofrece el área"* y en un camping *"Incluido en el
  precio"*. Viven en **otra pantalla**: los 320 px
  de alto ya van justos (48 de cabecera + 116 de las cinco casillas + 62 del
  precio + 54 de la fila de acciones = 280 de los 304 útiles), y seis
  casillas más no caben de ninguna manera. No llevan botón de guardar: se
  guardan con la parada. El botón "Servicios" comparte fila con "Guardar" en
  vez de llevar la suya, así no cuesta ni un píxel de alto.

  Esa pantalla sí va sobrada de alto, así que sus casillas van **más
  separadas** (`SERV_CHK_GAP`, 26 px frente a los 6 de mantenimiento y parada)
  y el bloque **centrado** en lo que sobra en vez de pegado arriba: más
  separación es menos fallo al tocar en marcha.

  **Valoración** (`build_valoracion`) es una **tercera pantalla**, colgada de
  servicios. Tres botones de un dedo con su color — **Recomendado** en verde,
  **Aceptable** en ámbar, **Sucio** en rojo — de los que solo uno queda
  elegido: el elegido va a todo color y con la marca de visto, los otros dos
  apagados. Dos señales a la vez y no una, porque solo con el borde no se
  distingue de lejos y solo con el color tampoco, y esto se mira con sol de
  lado. Debajo, las **pegas del sitio**: Ruidoso · Sin sombra, que son
  casillas y no notas porque pueden pasar con cualquiera de las tres (un sitio
  recomendable puede no tener sombra).

  Antes esto era una tira de tres botones que se desplegaba dentro de
  servicios y obligaba a apretar las casillas para hacerle sitio. En pantalla
  propia hay espacio de sobra: cabecera 48 + tres botones elásticos (~66 cada
  uno) + la fila de casillas 40 + 12 de huecos, de los 304 útiles.

  **Desmarcar Valoración borra lo que hubiera dentro** (nota y pegas): sin
  valoración no significan nada. Las pegas no caben en la casilla de
  servicios, así que donde se repasan es en el resumen de la confirmación,
  antes de guardar.

  Ese resumen **describe los servicios con su nombre entero** (no "6
  marcados", ni abreviados: "Vaciado grises" se entiende sin pensar y
  "Grises" no). Cabe porque **el diálogo ajusta la letra a lo que haya que
  contar** — 32, 24 o 20 según la longitud (`confirm_screen_open()`). Con la
  32 sólo entran cuatro líneas de ~25 caracteres y un resumen largo se salía
  por abajo empujando los botones fuera de la pantalla; con la 20 caben ~45
  caracteres por línea y el peor caso (todo marcado) se queda en cinco.

  **Iniciar y finalizar viaje YA se mandan a la P4** (22-ago-2026, probado de
  punta a punta: `{"op":"fin","id":3} -> HTTP 200` en la 3.5" y
  `VIAJE CERRADO: /sdcard/viajes/2026-08-22_zumaia` en la P4). Los demás
  botones todavía no envían nada — es la fase 3 del plan, por el mismo canal.
- **Fase 3** (hecho y validado en placa real el 21-ago-2026): **la zona
  aceptable es un ÓVALO, no un círculo**, porque un frigorífico de absorción no
  aguanta lo mismo en los dos ejes — **3° de lado a lado y 6° de morro a cola**
  (especificación de Dometic; avisan además de no dejarlo desnivelado más de
  1-2 h funcionando, que el amoniaco cristaliza y taponaría el circuito).
  Dibujarla redonda mentiría por los dos lados: asustaría de más cabeceando y
  de menos balanceando. Dentro, un círculo verde de **1°** = *NIVELADA*
  (cómodo para dormir y de sobra para el frigo; el 0,5° inicial era más fino
  que la precisión del propio montaje del sensor). Y **la bola rueda
  al lado BAJO**, como una canica — donde esté la bola, ahí va la rampa, sin
  traducir nada mentalmente. El balanceo ya salía así; el cabeceo iba al revés
  (las dos fórmulas de `tilt.c` no llevan el mismo signo) y se corrige en el
  dibujo, no en el sensor, para no invertir también el ángulo escrito ni la
  calibración guardada. Burbuja de nivel clásica (círculo que se desplaza, sin
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
- **Fase 4 — COMPLETA** (22-ago-2026). Canal de vuelta hacia la P4
  (`~/joint/victron`). Diseño en
  `docs/superpowers/specs/2026-08-22-viaje-con-nombre-y-carpeta-design.md`,
  cinco sub-fases, **las cinco hechas y probadas en la placa**:
  1. `POST /api/viaje` con Basic Auth estricta, inicio/fin, carpeta
     `AAAA-MM-DD_Destino` en la SD.
  2. Cola persistente en NVS con reintentos cada 15 s y pastilla de pendientes.
  3. Repostajes, peajes, bombonas, mantenimientos y paradas, cada uno a su CSV
     además del diario `eventos.csv`.
  4. Telemetría cada 5 min y contadores cada hora en la carpeta del viaje, más
     `resumen.txt` con totales al cerrar.
  5. `/data/viajes` con el estado de cada uno; los incompletos se detectan y se
     bloquean. El antiguo `viaje.tar` (que no era un viaje) pasa a
     `historico.tar`.

  **Pendiente, a la espera del GPS de la P4**: posición de cada apunte y
  kilómetros del viaje (ver la nota del diseño: la posición NO se puede sellar
  al recibir, por el mismo motivo que la hora).

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

> 📖 **[User manual](docs/MANUAL.md)** ([PDF](docs/MANUAL.pdf), Spanish) — how to use
> trip, stops, refuelling, tolls, gas bottles and maintenance, in plain language.
> Rebuild the PDF with `python3 docs/manual_a_pdf.py docs/MANUAL.md docs/MANUAL.pdf`.

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
  (ESP32-C6): UDP broadcast reception from the P4's Soft-AP (`ESP_DC078D`),
  `mini_msg_t` protocol (36 bytes, version 3, port 4242).

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
