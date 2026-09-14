#!/usr/bin/env python3
"""decodifica_capturas.py - saca PNGs de los volcados del carrusel de captura.

La 35cabina, con CAPTURE_CAROUSEL_ENABLE a 1, vuelca cada pantalla por el puerto
serie en bloques:

    ===CAPTURE:<nombre>:<ancho>x<alto>===
    <base64 de una linea>
    ...
    ===END===

El base64 es el framebuffer tal cual (rgb565, con el byte-swap de
CONFIG_LV_COLOR_16_SWAP), asi que aqui se deshace el swap y se escribe el PNG.

Uso:
    decodifica_capturas.py log.txt [-o carpeta] [--solo nombre ...]

Escribe <carpeta>/<nombre>.png. La carpeta por defecto es la del log.
"""
import argparse
import base64
import os
import re
import sys

from PIL import Image

CABECERA = re.compile(r"^===CAPTURE:([^:]+):(\d+)x(\d+)===$")
FIN = "===END==="


def rgb565_a_rgb(datos, ancho, alto, swap):
    px = []
    for i in range(0, ancho * alto * 2, 2):
        b0, b1 = datos[i], datos[i + 1]
        # CONFIG_LV_COLOR_16_SWAP: los dos bytes van al reves de lo que espera
        # el formato little-endian normal, asi que se intercambian.
        if swap:
            b0, b1 = b1, b0
        v = (b0 << 8) | b1
        r = (v >> 11) & 0x1F
        g = (v >> 5) & 0x3F
        b = v & 0x1F
        px.append(((r * 255) // 31, (g * 255) // 63, (b * 255) // 31))
    im = Image.new("RGB", (ancho, alto))
    im.putdata(px)
    return im


def capturas(ruta):
    """Genera (nombre, ancho, alto, base64_junto) de cada bloque del log."""
    nombre = None
    ancho = alto = 0
    trozos = []
    with open(ruta, "r", encoding="utf-8", errors="replace") as f:
        for linea in f:
            linea = linea.rstrip("\n")
            m = CABECERA.match(linea)
            if m:
                nombre, ancho, alto = m.group(1), int(m.group(2)), int(m.group(3))
                trozos = []
                continue
            if linea == FIN and nombre:
                yield nombre, ancho, alto, "".join(trozos)
                nombre = None
                trozos = []
                continue
            if nombre and linea and not linea.startswith("==="):
                trozos.append(linea.strip())


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("log")
    ap.add_argument("-o", "--salida")
    ap.add_argument("--solo", nargs="*", default=None,
                    help="nombres a extraer (por defecto, todos)")
    ap.add_argument("--sin-swap", action="store_true",
                    help="si la imagen sale con los colores cambiados de sitio")
    args = ap.parse_args()

    carpeta = args.salida or os.path.dirname(os.path.abspath(args.log))
    os.makedirs(carpeta, exist_ok=True)

    n = 0
    for nombre, ancho, alto, b64 in capturas(args.log):
        if args.solo and nombre not in args.solo:
            continue
        try:
            datos = base64.b64decode(b64, validate=True)
        except Exception as e:
            print(f"[!] {nombre}: base64 ilegible ({e}) -- bloque descartado")
            continue
        esperado = ancho * alto * 2
        if len(datos) != esperado:
            print(f"[!] {nombre}: {len(datos)} bytes, se esperaban {esperado} "
                  f"({ancho}x{alto}) -- bloque descartado")
            continue
        im = rgb565_a_rgb(datos, ancho, alto, not args.sin_swap)
        destino = os.path.join(carpeta, f"{nombre}.png")
        im.save(destino)
        print(f"[ok] {destino}  ({ancho}x{alto})")
        n += 1
    if n == 0:
        print("No se ha extraido ninguna captura. ¿Estaba CAPTURE_CAROUSEL_ENABLE a 1?")
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
