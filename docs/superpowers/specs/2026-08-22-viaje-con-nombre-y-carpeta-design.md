# Viajes con nombre y carpeta propia (Fase 4)

**Fecha:** 2026-08-22 · **Estado:** diseño, pendiente de revisión del usuario

## Qué se quiere

Al pulsar **Iniciar viaje** en la 3.5", que pida un **nombre**. A partir de ahí,
todo lo que se apunte durante ese viaje acaba en una carpeta propia de la SD de
la P4, llamada `AAAA-MM-DD_Nombre`, y **cada cosa que pase en la 3.5" viaja a la
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
| Empezar sin la P4 | Se permite; se sincroniza cuando aparezca |
| Canal | **HTTP** contra el portal de la P4 (TCP: no se pierde nada) |
| Telemetría | **En los dos sitios**: por días como siempre y copia en la carpeta |
| Parada abierta al finalizar | **Avisar y preguntar** antes de cerrar el viaje |
| Cola de pendientes | **Persistente**, sobrevive al apagado |
| Renombrar el viaje | **No**. Se avisa al escribirlo |

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
{ "op": "inicio", "id": 41, "nombre": "Galicia", "fecha_dias": 20687 }
{ "op": "registro", "id": 42, "tipo": "repostaje", "datos": { ... } }
{ "op": "fin", "id": 57 }
```

- `id` es un contador que lleva la 3.5", **creciente y sin huecos**.
- `fecha_dias` es el día de la P4 tal y como lo recibió la 3.5" (ver
  `mini_proto.h`). Si la 3.5" nunca tuvo reloj, se omite y **la P4 pone la fecha
  del momento en que lo recibe**.

**Idempotencia:** con reintentos, la P4 puede recibir lo mismo dos veces. Guarda
el último `id` aplicado y **descarta lo que ya haya aplicado**, respondiendo OK
igualmente para que la 3.5" lo dé por entregado y no se atasque.

### Estado y cola en la 3.5"

En NVS, namespace `viaje` (ya existe con `activo`):

- `nombre` — el del viaje en curso.
- `seq` — el contador de `id`.
- Cola: entradas `q<n>` como blobs, con índices de cabeza y cola.

**Por qué persistente:** la 3.5" se apaga con el contacto constantemente. Con la
cola solo en RAM, un repostaje apuntado con la P4 apagada se perdería al arrancar
el motor, que es el caso normal.

**Orden garantizado:** se envía siempre desde la cabeza. El `fin` entra en la
cola como uno más, así que **nunca adelanta** a los registros pendientes.

**Límite:** 64 entradas. Al llenarse se avisa en pantalla en vez de tirar nada en
silencio.

### En la P4

1. `op=inicio` → crea `/sdcard/viajes/AAAA-MM-DD_Nombre/` y lo marca como viaje
   abierto (en su NVS, para sobrevivir a reinicios).
2. `op=registro` → añade una línea al fichero del tipo correspondiente dentro de
   la carpeta (`repostajes.csv`, `peajes.csv`, `paradas.csv`...).
3. Telemetría: mientras haya viaje abierto, el `datalogger` escribe **además** en
   `<carpeta>/telemetria_AAAA-MM-DD.csv`. Sigue guardando en `/sdcard/frigo/`
   como siempre, para no romper el histórico continuo.
4. `op=fin` → vuelca lo pendiente, deja de duplicar la telemetría y escribe
   `resumen.txt`: días, kilómetros, litros repostados y gasto por conceptos.

### El nombre

- **Solo ASCII**, sin acentos ni eñes: las fuentes Montserrat compiladas no los
  traen y saldrían cuadrados (ver la cabecera de `view_registro.c`).
- Máximo 20 caracteres.
- Se filtran `/ \ : *` y demás, que romperían la ruta.
- Se pide con el editor a pantalla completa que ya existe (`entry_screen.c`).
- **Al escribirlo se avisa de que no se podrá cambiar.**

## Casos límite

| Situación | Qué hace |
|---|---|
| Iniciar viaje sin la P4 | Se acepta. Queda en cola; la carpeta se crea cuando aparezca |
| Sin reloj al iniciar | Se manda sin fecha y la P4 pone la del momento de recibirlo |
| Finalizar con parada abierta | Avisa: "Tienes una parada sin cerrar en X. ¿La cierro y termino?" |
| La P4 no aparece nunca | La cola aguanta 64 entradas y avisa al llenarse |
| Reenvío duplicado | La P4 lo descarta por `id` y responde OK |
| Nombre repetido el mismo día | La P4 añade un sufijo `_2` en vez de mezclar dos viajes |

## Lo que NO entra

- El log del sistema en la carpeta del viaje (decisión del usuario).
- Renombrar un viaje ya empezado.
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

1. **Canal + inicio/fin**: nombre, endpoint, carpeta y resumen vacío. Ya se ve
   la carpeta creándose sola.
2. **Cola persistente** y reintentos, con el aviso de pendientes en pantalla.
3. **Registros** (repostaje, peaje, bombona, mantenimiento, parada) al fichero
   que les toca.
4. **Telemetría duplicada** y **resumen** con totales.
