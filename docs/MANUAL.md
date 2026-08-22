# Manual de uso — pantalla de 3,5" de la cabina

Guía de las anotaciones del viaje: viaje, paradas, repostajes, peajes, bombonas
y mantenimiento. Escrita para usarla en la carretera, no para programar.

> **Antes de nada, lo importante:** de todo lo que anotas aquí, hoy **solo el
> inicio y el fin de viaje llegan a la P4**. Eso sí funciona: al empezar un viaje
> se crea su carpeta en la tarjeta de la P4 (probado el 22-ago-2026).
>
> **Los repostajes, peajes, bombonas, mantenimientos y paradas TODAVÍA NO se
> envían.** Se confirman en pantalla y se apuntan en el registro interno, pero
> **no llegan a la tarjeta**, y al apagar la pantalla lo tecleado desaparece. Lo
> único que se conserva entre encendidos es *si hay un viaje en marcha* y *si hay
> una parada abierta*. Así que **si un repostaje te importa, apúntalo también en
> otro sitio** hasta que esté la fase 3.

---

## 1. Moverse por la pantalla

Hay **tres pantallas** y se cambia **deslizando el dedo** a izquierda o derecha:

```
   INCLINACIÓN   <—>   DATOS   <—>   REGISTROS
    (nivelar)         (arranque)      (anotar)
```

Al encender siempre aparece **Datos**, la del centro.

Dos cosas que conviene saber:

- **Deslizar no es tocar.** Aunque empieces el gesto con el dedo encima de un
  botón, no se pulsa. Puedes deslizar desde cualquier punto sin miedo.
- **Al salir de Registros, se cierra lo que tuvieras abierto.** Si estabas a
  medio rellenar un formulario y te vas a otra pantalla, al volver encuentras el
  menú de iconos limpio, no el formulario a medias. Es a propósito: **lo que no
  guardaste, se pierde.**

---

## 2. Pantalla de Datos

Lo que manda la P4, una vez por segundo. No se toca nada aquí, solo se mira.

- **Batería** (la tarjeta grande de arriba): el dibujo de la batería se rellena
  según la carga y **cambia de color** con el nivel. Debajo, voltios y amperios.
  - Amperios en **verde**: está cargando.
  - Amperios en **naranja**: está gastando.
  - A la derecha, **MOTOR**: el voltaje de la batería de arranque.
- **Aguas** (abajo a la izquierda): cuatro tramos para el agua limpia
  (1/4 … 4/4) y el bloque de abajo para las **grises**, que se pone **rojo** y
  pone "LLENO" cuando lo están.
- **Temperaturas** (abajo a la derecha): frigo y exterior, cada una con una
  **flecha de tendencia**:
  - **Flecha naranja hacia arriba** — la temperatura está subiendo.
  - **Flecha azul hacia abajo** — se mantiene o está bajando.

  La flecha tarda un poco en cambiar, a propósito: mira la tendencia de fondo y
  no cada oscilación del sensor, para que no esté bailando todo el rato.

Si ves **"--"** en vez de números, es que **no está llegando nada de la P4**
(apagada, fuera de alcance o Wi-Fi mal configurado). Ver el apartado 9.

---

## 3. Pantalla de Inclinación (nivelar)

Un nivel de burbuja para aparcar. La bola **rueda hacia el lado bajo**: si se va
hacia ti, ese lado es el que hay que subir.

Colores:

| Color | Significado |
|---|---|
| 🟢 Verde | Nivelada (menos de 1°) |
| 🟡 Ámbar | Aceptable — hasta 3° de lado a lado, hasta 6° de morro a cola |
| 🔴 Rojo | Demasiado inclinada |

El límite de 3° de lado a lado **no es un capricho: es el del frigorífico**. Por
encima de eso el frigo de absorción trabaja mal.

**Botón "Calibrar nivel":** ponlo a cero cuando la autocaravana esté realmente
nivelada (comprobado con un nivel de burbuja de verdad). Corrige el hecho de que
la pantalla no esté montada perfectamente recta.

---

## 4. Registros: el menú

Seis casillas, cada una de su color:

| Casilla | Para qué |
|---|---|
| 🔵 **Viaje** | Empezar y terminar viaje, y anotar paradas |
| 🟢 **Repostaje** | Gasoil / gasolina |
| 🟣 **Peaje** | Un importe y ya |
| 🟠 **Bombona** | Compra de bombonas de gas |
| 🩵 **Mantenimiento** | Aceite, filtros, correa, ruedas |
| ⬜ **Wi-Fi** | Ajustes del aparato (gris a propósito: no anota nada) |

**Todos los formularios funcionan igual:**

1. Tocas un campo → se abre **a pantalla completa** con teclado grande.
   (No sale un teclado pequeño debajo: la pantalla es de 3,5" y no cabría.)
2. Rellenas y vuelves.
3. Pulsas **Guardar**.
4. Sale **"¿Es correcto?"** con el resumen de lo que has metido. Confirmas o
   corriges.

**Lo que NO te piden y por qué:** ni fecha, ni hora, ni coordenadas. Todo eso lo
sabe la P4 y las pondrá ella cuando se abra el envío. Teclearlas a mano en el
surtidor no aporta nada.

**Moneda:** por defecto EUR. Hay diez monedas de la Europa continental (GBP,
CHF, SEK, NOK, DKK, PLN, CZK, HUF, RON). La elegida **se reinicia a EUR** cada
vez que se vuelve a abrir el formulario.

---

## 5. Viaje

**Esta pantalla cambia según haya viaje en marcha o no.**

### Sin viaje

Solo un botón: **Iniciar viaje**. No hay "Finalizar": terminar lo que no ha
empezado no significa nada.

Al pulsarlo te pide **el destino** con el teclado grande: no un nombre para el
viaje, sino *a dónde vas*, que es mucho más fácil de contestar con el motor en
marcha. Con eso la P4 crea la carpeta del viaje en su tarjeta, con la fecha
delante para que se ordenen solas:

```
/sdcard/viajes/2026-08-22_Zumaia/
```

- Sin acentos ni ñ, y máximo 20 caracteres. La pantalla no te deja teclear otra
  cosa.
- **El nombre no se puede cambiar después.** Te avisa antes de empezar.
- **Hace falta la P4 encendida.** Si no, avisa ("Enciende la P4 primero") y no
  empieza: la carpeta lleva la fecha en el nombre y esta pantalla no tiene reloj.

El viaje **no se da por empezado hasta que la P4 lo confirma**. Si algo falla te
lo dice y todo se queda como estaba, en vez de poner "viaje en curso" mientras en
la tarjeta no hay nada.

### Con viaje en marcha

- **Anotar parada** — grande, arriba. Es lo que se usa cada día.
- **Finalizar parada** — solo aparece si hay una parada abierta (ver apartado 6).
- **Finalizar viaje** — pequeño, rojo, abajo del todo. Lejos del pulgar que
  viene de anotar, para no darle sin querer.

El título pasa a "VIAJE EN CURSO" y la casilla del menú también lo dice, así que
de un vistazo sabes si el viaje está abierto.

**El estado sobrevive a apagar la pantalla.** Se guarda en la memoria interna;
un corte de corriente no lo pierde.

---

## 6. Parada

Se llega desde **Viaje → Anotar parada**. Marca lo que hayas hecho:

| Casilla | Nota |
|---|---|
| Vaciado | Se puede combinar con cualquiera |
| Llenado | Se puede combinar con cualquiera |
| Agua potable | Va aparte de "Llenado": puedes parar **solo** por una fuente |
| Pernocta gratis | Sitio de parada — **excluyente** con Área y Camping |
| Área | Sitio de parada — **excluyente**, y pide precio |
| Camping | Sitio de parada — **excluyente**, y pide precio |

⚠️ **Los tres sitios son excluyentes entre sí**: has parado en un sitio, no en dos
a la vez. Marcar uno **desmarca automáticamente** el que hubiera. Y al
cambiar entre área y camping **se borra el precio**, porque no es comparable.

### Precio (solo Área y Camping)

Aparece una fila con **importe + moneda + tipo de cobro** en la misma línea:

- **Camping:** siempre **por noche**. El selector ni sale.
- **Área:** eliges **"noche"** o **"24 h"**. Esto hay que decirlo **al llegar**,
  que es cuando tienes el cartel delante y sabes cómo cobran.

> **La diferencia importa para la cuenta.** Por noche se cuentan cambios de día:
> llegas el viernes por la tarde y te vas el sábado por la mañana = **1 noche**.
> Por 24 h se cuentan periodos desde que entras, **redondeando hacia arriba**:
> 25 horas son **2 periodos**. Es como cobran ellos.

### Servicios

Botón **"Servicios →"**, disponible en Área y en Camping:

Agua potable · Vaciado grises · Vaciado WC · Electricidad · Duchas/WC · Basura

En **camping** el texto de arriba dice *"Incluido en el precio"*, porque ahí no
estás marcando lo que hay sino lo que ya has pagado.

### Valoración

Última opción dentro de Servicios. Tiene pantalla propia con tres botones
grandes de color:

🟢 **Recomendado** · 🟡 **Aceptable** · 🔴 **Sucio**

Y debajo, dos pegas sueltas: **Ruidoso** y **Sin sombra**. Son casillas aparte y
no notas, porque un sitio recomendable puede perfectamente no tener sombra.

---

## 7. Paradas de varios días

Aquí está la parte que más conviene entender.

Una parada en un **área, un camping o una pernocta** no termina cuando le das a
Guardar: termina **cuando te vas**, que puede ser días después y con la pantalla
apagada por medio (se va con el contacto).

Por eso la parada queda **abierta**, y **al volver a encender** sale solo:

```
        ¿Fin de la parada?

              Área
            3 noches
        Total:  45.00 EUR

  [ No, continuar ]  [ Sí, terminar ]
```

- **"Sí, terminar"** — cierra la parada con esa cuenta.
- **"No, continuar"** — **sigues otro día más ahí**. La parada se queda abierta.

Si dices que no, **no vuelve a preguntar sola**. Para cerrarla cuando quieras
está el botón **"Finalizar parada"** en la pantalla de Viaje.

Las paradas de **solo vaciado, llenado o agua** no dejan nada abierto: se acaban
en el sitio.

### ⚠️ Necesita que la P4 esté encendida

Esta pantalla **no tiene reloj propio** (ni pila). La fecha se la da la P4.

- Si guardas una parada y **la P4 no ha dado la hora todavía**, sale un aviso:
  *"Parada sin contar"*. La parada **no se abre**, porque no habría forma de
  contar las noches. Es mejor decírtelo que dejarte creer que se está contando.
- Al encender, si la P4 no aparece, **no pregunta nada** y sigue esperando. Más
  vale callar que inventarse las noches.

Como norma: **enciende la P4 antes que esta pantalla**, o al menos deja que se
enlacen antes de anotar la parada.

---

## 8. Los demás formularios

### Repostaje
Moneda · **Importe** y **Litros** en la misma línea · **Precio/litro**, que se
calcula solo mientras escribes.

### Peaje
Solo **Importe** y moneda. Es el más rápido de todos.

### Bombona
**Cuántas** (1 o 2) · **Precio total**.

### Mantenimiento
Seis casillas, y puedes marcar **varias a la vez** (con el mismo kilometraje
puedes haber hecho el aceite *y* su filtro):

Aceite · Filtro aceite · Filtro aire · Filtro habitáculo · Correa · Ruedas

Al marcar **Ruedas** aparece **cuántas** (1 a 4); si no, ese selector ni se ve.
Abajo, **Km** y **Coste**.

### Wi-Fi (Ajustes)
Cuatro campos, y **la pantalla se desliza hacia abajo** porque no caben todos a
la vez:

- **SSID** y **Password** — la red de la P4 a la que se conecta esta pantalla.
  Sirve para **cambiar a otra P4 sin reflashear**.
- **Usuario del portal** y **Clave del portal** — hacen falta para mandarle los
  apuntes del viaje. **No son los del Wi-Fi**: se ven en la P4, en Ajustes →
  Wi-Fi. Sin ellos, al iniciar un viaje sale "La P4 no acepta la clave".

Si tocas Guardar **sin haber cambiado nada**, te lo dice: *"No has cambiado
nada."* Y si sí lo has cambiado, **pide confirmación** antes de reconectar — es
fácil dejarse la pantalla incomunicada por un dedazo en la contraseña.

---

## 9. Cuando algo no va

| Qué ves | Qué pasa |
|---|---|
| **"--"** en todos los datos | No llega nada de la P4. Comprueba que esté encendida y que el Wi-Fi de Ajustes apunte a su red |
| **"Parada sin contar"** | Guardaste la parada antes de que la P4 diera la hora. Enciende la P4 y vuelve a anotarla |
| **"Sin la P4"** al finalizar parada | Lo mismo: sin fecha no se puede calcular la estancia |
| **"La P4 no acepta la clave"** | Falta el usuario o la clave del PORTAL en Ajustes (no son los del Wi-Fi) |
| **"Ya hay un viaje abierto"** | La P4 tiene uno sin cerrar. Termínalo antes de empezar otro |
| **"Sensor ADXL345 no detectado"** | El sensor de inclinación no responde. Es un problema de conexión, no de uso |
| El nivel marca torcido estando recta | Pulsa **Calibrar nivel** con la autocaravana bien nivelada |
| Volviste a Registros y estaba en blanco | Normal: al salir se limpia. Lo no guardado se pierde |

---

## 10. Resumen de lo que todavía no hace

Para no llevarse sorpresas:

1. **Solo se envía el inicio y el fin de viaje.** Ningún repostaje, peaje,
   bombona, mantenimiento ni parada llega todavía a la carpeta del viaje.
2. **No guarda el contenido de los formularios.** Solo *si hay viaje*, *cuál es
   el destino* y *si hay parada abierta*.
3. **No tiene reloj propio.** Sin la P4 no sabe qué día es.
4. **No lleva acentos ni ñ en pantalla.** Las letras que trae la fuente son solo
   las básicas; poner acentos sacaría cuadraditos vacíos. Decisión consciente:
   compilar fuentes nuevas era mucho lío para poco.

La fase 3 del envío se lleva por delante los puntos 1 y 2: todo lo anotado aquí
acabará en la carpeta del viaje, cada cosa en su fichero.
