# Montaje en la autocaravana (35cabina)

Procedimiento para instalar la pantalla de cabina y dejarla hablando con la P4.
Escrito el 7-oct-2026, cuando se sustituyó la unidad averiada por una nueva.

## Antes de llevarla

La placa sale del banco ya probada: arranca, se asocia al AP de la P4, recibe
telemetría, y la UI, el táctil y el brillo funcionan (100 % ↔ 30 % con doble
toque). El firmware que lleva es el **v2.16** o posterior (el que esté publicado
en la Release: `git describe --tags --match "v*.*"`).

## Lo que hay que hacer EN la autocaravana

### 1. Usuario y clave del PORTAL de la P4 (esto es lo único que puede fallar en silencio)

La NVS de la pantalla nueva viene vacía y **estas credenciales no tienen valor de
fábrica**: sin ellas, todo lo que la pantalla le manda a la P4 (iniciar/cerrar
viaje, apuntes) responde **401** y el aviso dice *"La P4 no acepta la clave.
Revisa usuario y clave en Ajustes"*.

- Se ven en la **P4**: Ajustes → Wi-Fi.
- Se meten en la **pantalla**: Ajustes → WiFi → *"Usuario del portal"* y
  *"Clave del portal"* (no son los del wifi; la propia pantalla lo avisa).
- Comprobación: iniciar un viaje de prueba y cerrarlo. Si la P4 lo acepta, la
  pantalla deja de dar el aviso y el viaje aparece en la carpeta de la P4.

### 2. Acelerómetro (ADXL345)

Va en su propio bus I2C, **SDA IO17 / SCL IO18**, con un cable de 4 hilos. Si no
está conectado, la pantalla avisa `ADXL345 no responde` en el log y la vista de
inclinación se queda sin datos.

- La calibración de la unidad anterior **no se puede recuperar** (se fue con la
  placa averiada): hay que volver a calibrarla con la pantalla ya montada y el
  vehículo en plano.
- Tiene que ir **rígidamente unido al vehículo** (no al soporte si este se
  orienta). Sensor tumbado = X hacia delante, Z hacia arriba.

### 3. Lo que va cableado (no se ve en el log)

- **Relés**: probar uno (luz INT/EXT o bomba) desde la pantalla.
- **Altavoz**: probar que suena una alarma.

### 4. La P4

La de la autocaravana tiene que llevar el arreglo del publicador UDP
(`udp_tx`: recrear el socket si se queda sin salida). Sin él, hay arranques en
los que la P4 se queda muda y la cabina no recibe nada hasta reiniciarla a mano.
Se graba por USB: `idf.py -p PUERTO flash` desde `~/joint/victron`.

## Qué mirar los primeros minutos

1. **Datos en pantalla** con la P4 encendida: batería/solar/DC-DC con sus cifras
   (o `--` en lo que la P4 no tenga, que es lo correcto si el shunt no está).
2. **Hora**: la de la P4, no la de arranque.
3. **Apagar la P4** y comprobar que en ~5 s las cifras pasan a `--` y el punto de
   enlace se pone gris (v2.16: los datos caducan, no se quedan congelados). Al
   encenderla, vuelven solas en unos 7 s.
4. **Si se cae el AP de la P4**: la pantalla reconecta sola. Medido con la P4
   reiniciándose: pierde el AP a los 3,6 s y vuelve a los 15,6 s.

## Operación normal (para tenerlo presente)

- La **P4 está siempre encendida**; la cabina se enciende y se apaga con el
  contacto. La cabina arranca en ~2 s y tiene datos a los ~7 s.
- El **auto-off de 15 minutos de la P4 solo para el servidor web**, no el AP: la
  radio sigue emitiendo y la cabina recibe aunque haya estado apagada horas.
- La cabina apagada **no** se interpreta como avería en la P4 (el watchdog vigila
  que su tarea de latido siga viva, no que la cabina hable).
