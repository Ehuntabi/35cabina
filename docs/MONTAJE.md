# Montaje en la autocaravana (35cabina)

Procedimiento para instalar la pantalla de cabina y dejarla hablando con la P4.
Escrito el 7-oct-2026, cuando se sustituyó la unidad averiada por una nueva.

## Antes de llevarla

La placa sale del banco ya probada: arranca, se asocia al AP de la P4, recibe
telemetría, y la UI, el táctil y el brillo funcionan (100 % ↔ 25 % con doble
toque). El firmware que lleva es el **v2.16** o posterior (el que esté publicado
en la Release: `git describe --tags --match "v*.*"`).

## Lo que hay que hacer EN la autocaravana

### 1. Usuario y clave del PORTAL de la P4 (esto es lo único que puede fallar en silencio)

La NVS de la pantalla nueva viene vacía y **estas credenciales no tienen valor de
fábrica**: sin ellas, todo lo que la pantalla le manda a la P4 (iniciar/cerrar
viaje, apuntes) responde **401** y el aviso dice *"La P4 no acepta la clave.
Revisa usuario y clave en Ajustes"*.

- Se ven en la **P4**: Ajustes → Wi-Fi.
- Se meten en la **pantalla**: Ajustes → WiFi → *"Usuario"* y
  *"Clave"* (no son los del wifi; la propia pantalla lo avisa).
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

## El canal de subida se prueba solo (v2.17 en adelante)

Al arrancar, la pantalla hace un **auto-test del canal hacia la P4** y lo deja en
el log, en castellano, sin crear ningún viaje:

```
p4_test: credenciales del portal en NVS: si, usuario 'victron'
p4_test: tcp 80: contesta (HTTP/1.1 401 Unauthorized      <- el portal esta vivo
p4_test: auth 80: CON credenciales -> HTTP/1.1 200 OK      <- las credenciales valen
p4_test: 3) CANAL DE VIAJES OK: la P4 ha contestado HTTP 400 al apunte de prueba (rechazado a proposito, no se ha creado nada)
p4_test: 4) CANAL DE ALARMAS OK: la P4 acepta la orden (HTTP 200; con mascara 0 no se ha callado ninguna alarma)
p4_test: 5) con credenciales malas la P4 contesta 401, como debe
```

Ojo con los dos códigos, que no significan lo mismo (revisión del 13-oct-2026):

- **Viajes**: la prueba manda una operación que la P4 no conoce, así que lo
  correcto es un **400 exacto** (ni 404, ni 413, ni 401: esos también son 4xx y
  antes se daban por buenos).
- **Alarmas**: se manda el cuerpo real (`{"alarmas":0}`) y se exige **2xx**. Un
  **400** aquí es un fallo: quiere decir "no entiendo esa orden", no "atendida".

Si sale `CANAL DE VIAJES ROTO`, o `401` donde debería salir un 4xx, los apuntes
del cuaderno no se guardarán: se arregla poniendo usuario y clave del portal en
Ajustes → WiFi.

(La sonda `1) sonda informativa sin credenciales` da un falso negativo **a
propósito documentado**: el cliente HTTP de la biblioteca de IDF no resuelve el
reto 401 de la P4, así que las peticiones de producción mandan la cabecera
`Authorization` a mano. El veredicto bueno son los pasos 3, 4 y 5.)

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

## Cuadro de pruebas del enlace (banco, 7-oct-2026)

Todo lo comprobable sin datos reales ni sondas, con las dos placas en el banco:

| Que se prueba | Como | Resultado |
|---|---|---|
| P4 -> cabina, telemetria | UDP :4242, contador y CRC | **OK**: 1 paquete/s, 0 corruptos |
| Centinelas de "sin dato" | P4 sin shunt + simulador | **OK**: se pintan como `--` |
| Cabina -> P4, latido | UDP :4243 | **OK**: la P4 lo registra con su IP |
| Cabina -> P4, viajes | POST /api/viaje (nivel ABIERTO del portal: usuario/clave del portal, no la de administracion) | **OK**: HTTP 400 a la operacion de prueba |
| Cabina -> P4, alarmas | POST /api/alarma | **OK**: HTTP 2xx con mascara 0 (un 400 aqui es fallo) |
| Credenciales malas | POST con clave incorrecta | **OK**: 401, el aviso de Ajustes saltaria |
| Contrato entre repos | `victron/test/protocolo_cabina.sh` (vive en el repo de la P4) | **OK**: 15 campos y 4 operaciones |
| Reinicio rapido de la P4 | reset y cronometro | **OK**: vuelve a los ~15 s |
| Arranque lento de la P4 | P4 tardando ~50 s en levantar el AP | **OK**: reintenta y entra sola |
| Apagon/encendido de la cabina | reset del satelite | **OK**: datos a los ~7 s |
| Portal HTTP parado | ver abajo | No pasa con la cabina encendida |

### El auto-off del portal de la P4 (por que NO hay que preocuparse)

`config_server_ap.c` para el servidor web a los 15 min sin actividad, **pero no
para si hay algun cliente asociado**, y la cabina lo esta mientras tiene
contacto. Comprobado en el banco con una P4 de prueba que tenia el plazo bajado
a 1 minuto: la cabina mantiene el portal vivo, y ademas el auto-off solo se
rearma con peticiones AUTENTICADAS o con un cliente que entra o sale (un 401 no
lo rearma, visto en `config_server_auth.c`).

O sea: con la cabina encendida, los apuntes salen; con la cabina apagada no hay
nada que enviar. El aviso que habia en el codigo ("la cola no se vacia nunca")
aplica a un caso que en la practica no se da.
