# Viajes con nombre y carpeta propia (Fase 4)

**Fecha:** 2026-08-22 · **Estado:** diseño, pendiente de revisión del usuario

## Qué se quiere

Al pulsar **Iniciar viaje** en la 3.5", que pida **a dónde vas**. A partir de ahí,
todo lo que se apunte durante ese viaje acaba en una carpeta propia de la SD de
la P4, llamada `AAAA-MM-DD_Destino`, y **cada cosa que pase en la 3.5" viaja a la
P4**: no solo el inicio y el fin, también cada repostaje, peaje, bombona,
mantenimiento y parada, incluida la parada que se cierra días después.

## Punto de partida (lo que hay hoy)

- La 3.5" **solo recibe**. No le manda nada a la P4. Sus botones de viaje
  escriben en el log interno y se pierden al apagar.
- La 3.5" **no usa su ranura microSD**: la tiene, pero ningún software la monta.
- La P4 sí tiene tarjeta y registros:
  - `trip_computer`: contadores de viaje (km, tiempo) con "Nuevo viaje" y
    "Finalizar". **Sin nombre.**
  - `datalogger`: escribe `/sdcard/frigo/AAAA-MM-DD.csv`, **un fichero por día**.
  - El log del sistema va aparte, a `/sdcard/logs/<día>/`.

Es decir: esto **abre la Fase 4** (canal de vuelta 3.5"→P4) y además reorganiza
dónde escribe la P4.

## Decisiones tomadas (usuario, 2026-08-22)

| Decisión | Elegido |
|---|---|
| Dónde se guarda | En la **SD de la P4** |
| Qué va en la carpeta | Registros del usuario + telemetría + resumen. **No** el log del sistema |
| Nombre de la carpeta | `2026-08-22_Galicia` (fecha delante, para que ordene sola) |
| Empezar sin la P4 | **No se deja**: la P4 arranca siempre antes, así que se exige y se avisa |
| Canal | **HTTP** contra el portal de la P4 (TCP: no se pierde nada) |
| Telemetría | **En los dos sitios**: por días como siempre y copia en la carpeta |
| Parada abierta al finalizar | **Avisar y preguntar** antes de cerrar el viaje |
| Cola de pendientes | **Persistente**, sobrevive al apagado |
| Renombrar el destino | **No**. Se avisa al escribirlo |

## Arquitectura

### Canal: HTTP contra el portal

La 3.5" hace `POST` a `http://192.168.4.1/...`. Sobre TCP, así que **se sabe con
certeza qué se entregó** — que es justo lo que necesita una cola de pendientes.
UDP se descartó: pierde paquetes sin avisar y habría que reinventar
confirmaciones y reintentos.

**Consecuencia:** desde el 21-ago el portal exige usuario y contraseña también
en el nivel abierto. La 3.5" necesita esas credenciales, guardadas en su NVS y
tecleadas una vez en su pantalla de Ajustes (igual que la clave del Wi-Fi). Se
leen en la P4, en Ajustes → Wi-Fi.

### Endpoints nuevos en la P4

Un único endpoint con JSON, que ya hay cJSON en el proyecto:

```
POST /api/viaje      Basic Auth (nivel estricto: esto ESCRIBE en la tarjeta)
```

Cuerpo, según la operación:

```json
{ "op": "inicio", "id": 41, "destino": "Galicia", "fecha_dias": 20687 }
{ "op": "registro", "id": 42, "tipo": "repostaje", "datos": { ... } }
{ "op": "fin", "id": 57 }
```

- `id` es un contador que lleva la 3.5", **creciente y sin huecos**.
- `fecha_dias` es el día de la P4 tal y como lo recibió la 3.5" (ver
  `mini_proto.h`). **Siempre va**: un viaje no puede empezar sin reloj (ver
  abajo).

**Idempotencia:** con reintentos, la P4 puede recibir lo mismo dos veces. Guarda
el último `id` aplicado y **descarta lo que ya haya aplicado**, respondiendo OK
igualmente para que la 3.5" lo dé por entregado y no se atasque.

### La P4 arranca siempre antes que la 3.5"

Dato del usuario (2026-08-22), y simplifica el diseño: la 3.5" tendrá el reloj de
la P4 a los pocos segundos de encenderse, así que **lo raro deja de ser lo
normal**.

Consecuencia directa: **iniciar un viaje EXIGE la P4**. Si al pulsar todavía no
hay reloj, se esperan unos segundos y, si no aparece, se avisa ("Enciende la P4
primero") en vez de empezar a medias. Eso elimina de raíz tres casos límite que
en la práctica no se iban a dar nunca: el viaje sin fecha, la carpeta creada a
posteriori y la hora aproximada del propio inicio.

Lo que **NO** se quita es la cola: la P4 puede apagarse, colgarse o quedarse sin
cobertura a mitad de viaje, y sin cola un repostaje apuntado en ese hueco se
perdería sin avisar. Pasa de ser el camino habitual a ser una red de seguridad.

### El "cuándo": cada cosa se sella al ocurrir, no al entregarse

**El punto débil del primer borrador.** Caso normal: repostas con la P4 apagada,
el registro se queda en cola y sale dos días después al arrancar el motor. Si la
hora la pusiera la P4 al recibirlo, ese repostaje figuraría **dos días tarde**.

Así que la 3.5" sella cada evento en el momento en que ocurre: guarda el último
`epoch_local` que le dio la P4 junto con el `esp_timer` de ese instante, y para
sellar suma el tiempo transcurrido desde entonces. No necesita reloj propio, solo
recordar cuándo supo la hora por última vez.

Si en todo el arranque **nunca** ha visto a la P4, no hay de dónde sacar la hora:
el evento se marca como **hora aproximada** y la P4 le pone la de recepción. Va
con una marca en el fichero, para que al leerlo se sepa que ese dato no es
exacto en vez de creerselo.

### Estado y cola en la 3.5"

En NVS, namespace `viaje` (ya existe con `activo`):

- `destino` — el del viaje en curso.
- `seq` — el contador de `id`.
- Cola: entradas `q<n>` como blobs, con índices de cabeza y cola.

**Por qué persistente:** la 3.5" se apaga con el contacto constantemente. Con la
cola solo en RAM, un repostaje apuntado con la P4 apagada se perdería al arrancar
el motor, que es el caso normal.

**Orden garantizado:** se envía siempre desde la cabeza. El `fin` entra en la
cola como uno más, así que **nunca adelanta** a los registros pendientes.

**Límite:** 64 entradas. Al llenarse se avisa en pantalla en vez de tirar nada en
silencio.

### Aviso de pendientes en la 3.5"

Las paradas y los repostajes se apuntan **antes de apagar**, así que hay que
poder ver de un vistazo si queda algo sin enviar antes de quitar el contacto.

**Matiz importante:** apagar con pendientes **no pierde nada** — la cola está en
NVS y sobrevive. El riesgo real es irse a casa creyendo que el viaje está entero
y bajarlo antes de que se sincronice, y eso ya lo corta el bloqueo de los
incompletos. El aviso sirve para no llegar a esa situación.

**Limitación física:** la 3.5" **no puede saber que vas a quitar el contacto**;
se queda sin corriente y ya. Así que el aviso no puede saltar "al apagar": tiene
que estar visible **todo el rato** mientras quede algo pendiente.

Dos sitios:

1. **Pastilla en la pantalla principal**, abajo, solo cuando hay algo: *"2 sin
   enviar"*. Es la pantalla que está puesta cuando vas a quitar el contacto, así
   que es donde de verdad se ve. Desaparece sola al vaciarse la cola.
2. **En la confirmación al guardar**, si no se pudo entregar en el momento:
   *"Guardado. Pendiente de enviar: la P4 no responde"*. Te enteras en el acto,
   sin un cartel aparte que haya que cerrar en mitad del surtidor.

### En la P4

1. `op=inicio` → crea `/sdcard/viajes/AAAA-MM-DD_Destino/` y lo marca como viaje
   abierto (en su NVS, para sobrevivir a reinicios).
2. `op=registro` → añade una línea al fichero del tipo correspondiente dentro de
   la carpeta (`repostajes.csv`, `peajes.csv`, `paradas.csv`...).
3. Telemetría: mientras haya viaje abierto, el `datalogger` escribe **además** en
   `<carpeta>/telemetria_AAAA-MM-DD.csv`. Sigue guardando en `/sdcard/frigo/`
   como siempre, para no romper el histórico continuo.
4. **Contadores cada hora** en `contadores.csv`: kilómetros, tiempo y consumo del
   `trip_computer`. Si el viaje se corta a lo bruto (batería, avería) no se
   pierde el recorrido, y además se ve la evolución.
5. `op=fin` → vuelca lo pendiente, deja de duplicar la telemetría y escribe
   `resumen.txt`: días, kilómetros, litros repostados y gasto por conceptos.

### Qué hay dentro de la carpeta

```
/sdcard/viajes/2026-08-22_Galicia/
├─ eventos.csv          ← el DIARIO: todo en orden y con su hora
├─ repostajes.csv
├─ peajes.csv
├─ bombonas.csv
├─ mantenimientos.csv
├─ paradas.csv          ← con la nivelación con que se aparcó
├─ contadores.csv       ← km/tiempo/consumo, una línea por hora
├─ telemetria_2026-08-22.csv   (uno por día)
└─ resumen.txt          ← totales, al cerrar
```

`eventos.csv` es el que se lee para saber **qué pasó**: inicio, cada registro,
cada parada abierta y cerrada, el fin, todo seguido. Los CSV por tipo son para
hacer cuentas. Se escriben las dos cosas porque sirven para cosas distintas.

En `paradas.csv` va también **cómo quedó nivelada** la autocaravana (cabeceo y
balanceo al guardarla): es un dato del sitio, no del momento — si un área tiene
mucha pendiente, conviene saberlo antes de volver.

### El destino

La carpeta es **fecha de inicio + destino**: `2026-08-22_Galicia`. La fecha
delante para que las carpetas se ordenen solas en el ordenador, y el destino
detrás para reconocer el viaje de un vistazo.

No se pide "un nombre para el viaje" sino **a dónde vas**, que es mucho más
directo de contestar con el motor en marcha: el rótulo del teclado dice
**"Destino"**.

- **Solo ASCII**, sin acentos ni eñes: las fuentes Montserrat compiladas no los
  traen y saldrían cuadrados (ver la cabecera de `view_registro.c`). O sea
  "Bilbao" y "A Coruna", no "Bilbaó" ni "Logroño".
- Máximo 20 caracteres.
- Se filtran `/ \ : *` y demás, que romperían la ruta.
- Se pide con el editor a pantalla completa que ya existe (`entry_screen.c`).
- **Al escribirlo se avisa de que no se podrá cambiar.**

## Casos límite

| Situación | Qué hace |
|---|---|
| Iniciar viaje sin la P4 | **No se deja.** Espera unos segundos y avisa: "Enciende la P4 primero" |
| La P4 se cae a mitad de viaje | Lo apuntado se queda en cola y sale cuando vuelva |
| Finalizar con parada abierta | Avisa: "Tienes una parada sin cerrar en X. ¿La cierro y termino?" |
| La P4 no aparece nunca | La cola aguanta 64 entradas y avisa al llenarse |
| Reenvío duplicado | La P4 lo descarta por `id` y responde OK |
| Mismo destino el mismo día | La P4 añade un sufijo `_2` en vez de mezclar dos viajes |

## Descarga del viaje por Wi-Fi

Hoy la P4 ya sirve un `viaje.tar` en su portal, **pero no es un viaje**: empaqueta
`/sdcard/bateria`, `/sdcard/solar` y `/sdcard/frigo`, o sea el histórico entero.
El nombre engaña desde que existe.

Con esto:

- **`viaje.tar` pasa a ser de verdad un viaje**: la carpeta completa, con los
  registros, la telemetría y el resumen dentro.
- El paquete de siempre **no se pierde**: pasa a llamarse `historico.tar`. El
  analizador del PC sigue teniendo lo suyo y los nombres dejan de mentir.
- En la página de descargas sale **la lista de viajes guardados**, ordenados por
  fecha, y se elige cuál bajarse. Con varios viajes en la tarjeta es lo único
  práctico: `/data/viaje/2026-08-22_Galicia.tar`.

Se reaprovecha `tar_stream_dir()`, que ya sabe meter una carpeta con su prefijo
dentro de un `.tar` en streaming, y ya respeta el cerrojo de la SD con la cámara.

#### Solo se descarga lo que está COMPLETO

Un viaje solo se puede bajar cuando la P4 **sabe** que no le falta nada. En el
propio código del `.tar` ya hay un aviso sobre esto: *"el analizador del PC se
traga un viaje incompleto creyendo que está entero"*.

Cómo lo sabe: el mensaje de `fin` lleva **cuántos eventos ha mandado la 3.5" en
total**, y la P4 comprueba que los tiene todos **sin huecos** en los `id`. Solo
entonces lo marca como descargable.

En la lista, cada viaje sale con su estado:

| Estado | Qué significa | Descarga |
|---|---|---|
| **En curso** | No se ha finalizado | Bloqueada |
| **Incompleto (faltan N)** | Cerrado, pero faltan eventos | Solo la salida de emergencia |
| **Listo** | Cerrado y sin huecos | Normal |

**Salida de emergencia:** si se perdió algo para siempre (la cola se llenó, un
reinicio en mal momento), el viaje quedaría bloqueado eternamente. Hay un enlace
aparte que avisa de qué falta y descarga el paquete como
`2026-08-22_Galicia_INCOMPLETO.tar`. Así no te quedas sin tus datos y es
imposible confundirlo con uno entero.

## Lo que NO entra

- El log del sistema en la carpeta del viaje (decisión del usuario).
- Cambiar el destino de un viaje ya empezado.
- Que la 3.5" guarde nada en su propia microSD: sigue sin usarse.

## Riesgos

1. **Se toca el camino de escritura de la SD de la P4**, que tiene trampas
   conocidas: el cerrojo con la cámara (`camera_sd_bus_lock`), la SD que se queda
   pillada tras reinicios por USB. Cualquier escritura nueva debe pasar por
   `sd_safe` y respetar ese cerrojo.
2. **Credenciales en la 3.5"**: si cambian en la P4, la 3.5" deja de sincronizar
   hasta que se actualicen. Debe decirlo claramente, no fallar en silencio.
3. **Alcance**: son dos repos, un protocolo nuevo y un formato de ficheros. Se
   hará por fases, empezando por inicio/fin, que es lo que da valor antes.

## Fases propuestas

1. **Canal + inicio/fin**: destino, endpoint, carpeta y resumen vacío. Ya se ve
   la carpeta creándose sola.
2. **Cola persistente** y reintentos, con el aviso de pendientes en pantalla.
3. **Registros** (repostaje, peaje, bombona, mantenimiento, parada) al fichero
   que les toca.
4. **Telemetría duplicada** y **resumen** con totales.
5. **Descarga**: `viaje.tar` por viaje, lista con el estado de cada uno, bloqueo
   de los incompletos con su salida de emergencia, y renombrar el paquete viejo
   a `historico.tar`.
