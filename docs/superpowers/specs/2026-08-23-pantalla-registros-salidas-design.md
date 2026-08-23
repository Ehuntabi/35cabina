# La pantalla de registros, reconstruida alrededor de la salida

**23-ago-2026. Diseño aprobado.**

**Estado:** el estado en NVS (`main/salida.{c,h}`) y **las siete pantallas de
menú** están hechos. Falta la parte de **«Qué pasa al arrancar»** —rellenar lo
que quedó abierto, prolongar/finalizar parada y el aviso de la parada
olvidada— y los cambios de la P4.

Sustituye al diseño anterior del mismo día (proponía ocho casillas en el menú),
borrado por equivocado. Parte de `docs/menus.txt`, escrito por el usuario, más
las decisiones que se cerraron después y que constan aquí.

Afecta **solo a la pantalla de la derecha** del carrusel. Inclinación e Info se
quedan como están.

## La idea que lo sostiene todo

La 3.5" **se apaga al quitar el contacto** (`README.md:139`). Eso parecía un
estorbo y resulta ser el mecanismo:

> **Declaras al llegar, rellenas al salir.**

Con el motor todavía en marcha pulsas "Repostaje". Echas gasolina. Al girar la
llave, la pantalla arranca, ve que dejó un repostaje abierto y te pide importe,
litros y kilómetros — que es justo cuando ya los sabes.

De ahí salen dos propiedades que no hay que programar, vienen solas:

- **No hay nada que acordarse de cerrar.** Lo pregunta el vehículo al arrancar.
- **La hora de inicio y la de fin son reales**, no tecleadas.

No hace falta detectar el motor por voltaje. El arranque de la pantalla ES el
contacto. (Se comprobó que la P4 sabe deducir el alternador —entrada del Orion
por encima de 13,30 V, `view_default_battery.c:354`— y que ese dato ya viaja en
el protocolo. No se usa: el arranque es más fiable y no cuesta nada.)

## Los dos tipos de salida

La autocaravana se mueve por un motivo, y el viaje es solo uno de ellos:

- **Viaje** — dura días, tiene nombre y carpeta propia, y dentro caben muchos
  apuntes.
- **Puntual** — sales a repostar, a la ITV, a por una bombona o al taller.
  Un apunte y se acabó.

## Las pantallas

### Principal, sin salida en marcha

```
        [ NUEVA SALIDA ]        <- grande, ocupa la pantalla
        [ Configuración ]       <- mucho más pequeño
```

### Tipo de salida

`Viaje` o `Puntual`. Si es viaje, pide un nombre corto y crea la carpeta
`nombre_AAAAMMDD`.

### Menú de salida — el principal mientras dure el viaje

```
        [ AÑADIR PARADA ]       <- grande
        [ Terminar salida ]     <- mediano
        [ Configuración ]       <- el más pequeño
```

### Dentro de un viaje, seis cosas

| | Se declara al llegar | Se rellena al arrancar |
|---|---|---|
| **1 Parada** | motivo; si es pernocta, dónde | servicios, precio, valoración |
| **2 Aguas** | wc / agua / vaciado | gratis o coste de cada uno |
| **3 Repostaje** | un toque | importe, litros y **km** |
| **4 Peaje** | clase e importe, **en el momento** | — |
| **5 Bombona** | un toque | cuántas y precio por unidad |
| **6 Avería/Mant.** | qué se hizo | coste |

El **peaje** es la excepción a propósito: se hace con el motor en marcha y lo
rellena el copiloto.

### Puntual, cuatro cosas

Repostaje · Bombona · ITV · Mantenimiento/Avería. Al confirmar se vuelve al
menú principal: la salida puntual se acaba ahí.

La **ITV** al cerrarse pide kilómetros, precio y resultado.

## Las cuatro decisiones que no estaban en `menus.txt`

**1. La pernocta se parte en dos.** Al llegar, un toque: parking / área /
camping, y gratis o de pago. Nada más — llegas de noche, cansado y con el motor
en marcha, y ahí no se rellena un formulario de siete campos. Servicios, precio
y valoración se piden **al marcharte**, que además es cuando de verdad los
sabes: si la luz iba, si había agua y qué te pareció el sitio.

Es la misma lógica que ya hace funcionar al repostaje, aplicada donde más falta
hacía.

**2. Puede haber varias cosas abiertas a la vez.** Cinco días en un camping y
sales a repostar: prolongas la parada y abres un repostaje. Al arrancar se
pregunta **por cada una, en orden de apertura**. Sin esto, en cualquier estancia
larga se perderían los apuntes del medio.

**3. Si se te olvida declarar la parada, no se pierde.** La pantalla apunta la
hora **cada 10 minutos** mientras está viva. Al arrancar, esa marca es el
momento del apagón con un margen de ±10 minutos, y ofrece: *"estuviste parado
desde las 19:40, ¿anoto una parada?"*.

No puede guardarla "al apagarse" porque muere de golpe, sin aviso. De ahí la
marca periódica. Para una hora de aparcamiento, ±10 minutos da igual.

**4. El repostaje pide kilómetros.** Con eso salen solos los litros a los cien y
el coste por kilómetro. Si no se pide desde el primer día, los repostajes viejos
no lo tendrán nunca.

## Qué pasa al arrancar la pantalla

En este orden:

1. ¿Quedó algo abierto? → preguntar por cada cosa, en orden.
2. ¿Hubo un apagón largo, hay viaje en marcha y nada abierto? → ofrecer anotar
   la parada.
3. ¿Hay salida en marcha? → menú de salida.
4. Si no → menú principal.

Todo el estado vive en NVS, porque entre un paso y el siguiente el aparato ha
estado sin corriente.

## Las coordenadas: sin tocar el protocolo

`menus.txt` las pide en todos los apuntes. La 3.5" **no sabe dónde está** —
recibe si hay GPS, no la posición.

Pero no le hace falta: manda el apunte a la P4, y **la P4, que sí tiene el GPS,
le pone las coordenadas al escribirlo**. Nos ahorramos subir `mini_proto.h` a v5
y regrabar las dos pantallas a la vez.

**Con una salvedad:** si el apunte venía en la cola de pendientes (la P4 estaba
apagada o fuera de alcance), se escribe **sin coordenadas** en vez de ponerle el
sitio donde se entregó. Una posición equivocada es peor que ninguna — el mismo
criterio que ya se aplica al validar las tramas del GPS.

## En la P4

- Un apunte de **viaje** va a la carpeta del viaje, como hasta ahora.
- Un apunte **puntual** va a `/sdcard/vehiculo/<tipo>s.csv`, el historial del
  vehículo.
- `op=registro` deja de responder 409 cuando no hay viaje abierto.

## Lo que hay que aceptar

**La pantalla solo vive con el contacto puesto.** No se puede consultar el
viaje, ver totales ni corregir una errata estando aparcado: todo ocurre en el
rato en que la llave está girada. No es un fallo del diseño, es de dónde cuelga
la corriente.

## Tamaño del trabajo

`view_registro.c` son 2.100 líneas y se reestructura casi entera. Los
formularios que ya existen (repostaje, peaje, bombona, mantenimiento, servicios
y valoración de una parada) se reaprovechan; lo que se rehace es la navegación y
el estado que sobrevive al apagón. Más los cambios de la P4.

No es una tarde.
