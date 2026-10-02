# Material del fabricante (JC3248W535)

El paquete completo que publica Guition para este módulo (demos de Arduino y
ESP-IDF, especificaciones, datasheets, manual de usuario y utilidades) **no vive
en este repositorio**: pesa ~1,1 GB y la mitad son binarios y demos que no
usamos. Se guarda al lado, con la documentación del taller:

    ~/joint/documentacion/jc3248w535_material_fabricante/
      1-Demo/                      demos (Arduino, ESP-IDF, PlatformIO...)
      2-Specification/             JC3248W535 Specifications-EN.pdf
      3-Structure_Diagram/         cotas y dibujos de la placa
      4-Driver_IC_Data_Sheet/      AXS15231 (pantalla), ESP32-S3-WROOM-1,
                                   AX98357A y NS4168 (audio)
      5-IO pin distribution/       JC3248W535-1.png y -2.png
      6-User_Manual/               Getting started
      7-Character&Picture_Molding_Tool/
      8-Burn operation/

Del material del fabricante, en este repositorio se guardan **solo los dos
esquemáticos** (que son los que hacen falta para medir y reparar):

- `JC3248W535_V1.0_esquematico_p1.png` — alimentación (VOUT-BAT, buck U3, riel
  de 3,3 V con S3/S4, IP5306, USB-C) y conectores (P1 UART de 4 pines con 5 V,
  P2 Extended IO de 8, P4 de 4 con 3,3 V/IO17/IO18).
- `JC3248W535_V1.0_esquematico_p2.png` — ESP32-S3-WROOM-1 (mapa de GPIO),
  pantalla QSPI, táctil, microSD, cargador IP5306 con la batería (P5) y el
  amplificador de audio.

Regla de taller: las descargas gordas del fabricante van a `documentacion/`, no
dentro de los repos. `publicar.sh` hace `git add -A`, así que lo que esté dentro
del repo se commitea y se sube (pasó el 2-oct-2026 con este mismo paquete:
4.528 ficheros; se deshizo antes de subirlo).
