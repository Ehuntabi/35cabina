#!/usr/bin/env bash
#
# release.sh — publica una versión de la 3,5" de cabina.
#
# Uso:  ./release.sh X.Y  ["mensaje del tag"]
#   ej: ./release.sh 1.9.8 "El icono del GPS ya no cuelga del shunt"
#
# Qué hace:
#   1. Exige el árbol limpio (lo que se publica tiene que ser lo que hay).
#   2. Crea el tag vX.Y, o reutiliza el que ya esté EN ESTE MISMO commit.
#   3. Reconfigura y compila -- el reconfigure no es opcional: la versión se
#      calcula al CONFIGURAR, no al compilar, así que sin él se publicaría un
#      binario con la versión anterior dentro.
#   4. VERIFICA que la versión embebida en el .bin coincide con el tag.
#   5. Sube código y tag, y publica la Release en GitHub con el binario.
#      Sin 'gh' no se publica: se exige ANTES de crear el tag, y al final se
#      comprueba que la Release lleve su .bin de verdad.
#
# Existe porque este proyecto no tenía ninguno: 20 etiquetas creadas y CERO
# Releases publicadas. Y en el proyecto hermano, que sí lo tenía, el script
# solo IMPRIMÍA los comandos de publicar y había que copiarlos a mano: acabó
# con 74 tags y la última Release en la v1.5.5.
set -euo pipefail
cd "$(dirname "$0")"

REPO="Ehuntabi/35cabina"
# Un solo entorno para los tres proyectos (7-oct-2026): la P4 y la pantalla de 5"
# ya iban por 5.5.5, y mantener dos IDF en el mismo PC solo daba sorpresas de PATH
# (y un CI distinto). Este proyecto vivia en 5.4.4 por herencia de la saga del
# hang del LCD/SD; se migro a 5.5.5 y se valido en placa.
IDF_EXPORT="${IDF_EXPORT:-$HOME/.espressif/esp-idf-5.5/export.sh}"
RELDIR="$HOME/joint-releases"
APP_BIN="build/35cabina.bin"

[ $# -ge 1 ] || { echo "Uso: ./release.sh X.Y [\"mensaje\"]"; exit 1; }
VER="${1#v}"                                  # quita una 'v' inicial si la hay
# DOS numeros, no tres (24-ago-2026, mismo criterio que el proyecto hermano de
# la P4). Con tres se acababa gastando un numero por commit; con dos, una
# version es una TANDA de trabajo. Aqui ANTES NO SE VALIDABA NADA: colaba
# cualquier cosa como version, incluido un dedazo.
if ! printf '%s' "$VER" | grep -Eq '^[0-9]+\.[0-9]+$'; then
    echo "ERROR: '$1' no tiene el formato X.Y (ej: 1.10)."
    echo "       Desde el 24-ago-2026 las versiones llevan DOS numeros, no tres."
    exit 1
fi
TAG="v$VER"
MSG="${2:-Release $TAG}"

# ── 0b) hace falta 'gh': sin él no hay Release ──────────────────────────────
# Se mira AQUÍ, antes de crear el tag y de compilar, que es cuando abortar no
# cuesta nada. Antes se miraba al final y, si faltaba 'gh', el script se quedaba
# en "AVISO" y seguía: código y tag subidos, Release sin publicar, o sea una
# publicación a medias (auditoría del 15-oct-2026).
if ! command -v gh >/dev/null 2>&1; then
    echo "ERROR: no está 'gh' (GitHub CLI) y sin él no se puede publicar la Release."
    echo "       Instálalo (https://cli.github.com), entra con 'gh auth login' y"
    echo "       vuelve a lanzar ./release.sh $1. Todavía no se ha tocado nada."
    exit 1
fi

# ── 1) el árbol, limpio ─────────────────────────────────────────────────────
if [ -n "$(git status --porcelain)" ]; then
    echo "ABORTADO: hay cambios sin commitear. Lo que se publica tiene que ser"
    echo "          exactamente lo que hay en el repositorio."
    git status --short
    exit 1
fi

# ── 2) el tag ───────────────────────────────────────────────────────────────
if git rev-parse -q --verify "refs/tags/$TAG" >/dev/null; then
    if [ "$(git rev-list -n1 "$TAG")" != "$(git rev-parse HEAD)" ]; then
        echo "ABORTADO: el tag $TAG existe y apunta a OTRO commit."
        exit 1
    fi
    echo "[ok] el tag $TAG ya está en este commit, lo reutilizo"
else
    git tag -a "$TAG" -m "$MSG"
    echo "[ok] tag $TAG creado"
fi

# ── 3) compilar ─────────────────────────────────────────────────────────────
# shellcheck disable=SC1090
. "$IDF_EXPORT" >/dev/null 2>&1

# Mismo IDF PARCHEADO que la P4 (7-oct-2026): el arbol de 5.5 lleva los parches
# de ~/joint/victron/patches (memcpy NULL en el SPI del P4). Aqui se aplican
# tambien -- es idempotente -- para que los dos proyectos compilen contra el
# MISMO arbol y no haya "en mi PC funciona". Si el repo de la P4 no esta al
# lado, se avisa y se sigue: este firmware no usa ese camino del SPI.
PARCHE="$HOME/joint/victron/scripts/aplicar_parche_idf.sh"
if [ -x "$PARCHE" ]; then
    "$PARCHE" >/dev/null && echo "[ok] parches del IDF aplicados (los mismos que la P4)"
else
    echo "AVISO: no encuentro $PARCHE; compilo con el IDF tal cual"
fi

idf.py reconfigure >/dev/null
idf.py build >/dev/null
echo "[ok] compilado"

# ── 4) verificar que la versión embebida == tag ─────────────────────────────
EMB="$(python3 - "$APP_BIN" <<'PY'
import sys
with open(sys.argv[1],'rb') as f: d=f.read(0x120)
print(d[0x20+16:0x20+48].split(b'\x00')[0].decode('ascii','replace'))
PY
)"
if [ "$EMB" != "$TAG" ]; then
    echo "ABORTADO: el binario lleva '$EMB' y el tag es '$TAG'. No se publica"
    echo "          un firmware que dice ser otra versión."
    exit 1
fi
echo "[ok] el binario dice '$EMB'"

mkdir -p "$RELDIR"
rm -f "$RELDIR"/35cabina-v*.bin
OUT="$RELDIR/35cabina-$TAG.bin"
cp "$APP_BIN" "$OUT"
echo "[ok] binario en $OUT"

# ── 5) publicar ─────────────────────────────────────────────────────────────
git push origin "$(git branch --show-current)" "$TAG"

if ! command -v gh >/dev/null 2>&1; then
    # No debería llegar aquí (se comprueba en el paso 0b, antes de crear el tag),
    # pero si pasa NO se sigue: el código y el tag YA están subidos, así que
    # dejarlo así es una publicación a medias, que es lo que este script evita.
    echo "ERROR: 'gh' ha desaparecido a mitad de la publicación (¿el PATH?)."
    echo "       El código y el tag $TAG YA están subidos; falta la Release. Instala"
    echo "       'gh', entra con 'gh auth login' y publícala con:"
    echo "       gh release create $TAG -R $REPO --title \"35cabina $TAG\" \\"
    echo "          --notes-from-tag \"$OUT\""
    exit 1
fi
if gh release view "$TAG" -R "$REPO" >/dev/null 2>&1; then
    echo "[ok] la Release $TAG ya existe, no la toco"
else
    NOTAS_TMP="/tmp/notas-35cabina-$TAG.md"
    # Si se paso un mensaje a mano (segundo argumento), ese es la nota. Si no,
    # el mensaje del tag se queda en el generico "Release vX.Y" (linea 41) y
    # una Release con eso no dice nada -- paso con la v1.14 (07-sep-2026):
    # se publico practicamente en blanco. Mismo criterio que el proyecto
    # hermano (victron/release.sh): sin mensaje a mano, las notas salen del
    # HISTORIAL, no de un texto vacio.
    if [ -n "${2:-}" ]; then
        printf '%s\n' "$MSG" > "$NOTAS_TMP"
    else
        ANTERIOR=$(git describe --tags --abbrev=0 --match "v*.*" "$TAG^" 2>/dev/null || true)
        {
            if [ -n "$ANTERIOR" ]; then
                echo "Cambios desde $ANTERIOR:"
                echo
                git log --no-merges --format='- %s' "$ANTERIOR..$TAG"
            else
                echo "Primera version publicada."
            fi
        } > "$NOTAS_TMP"
    fi
    if gh release create "$TAG" -R "$REPO" \
         --title "35cabina $TAG" \
         --notes-file "$NOTAS_TMP" \
         "$OUT"; then
        :
    else
        echo "AVISO: GitHub no ha creado la Release (¿un 5xx suyo? paso el 16-oct-2026)."
    fi
    # SE COMPRUEBA, no se da por hecho: si la Release no existe, el tag esta
    # subido y el .bin en ~/joint-releases, pero nadie puede descargarlo -- y
    # scripts/grabar.sh no dejaria grabar la placa. Mejor decirlo aqui, con el
    # comando exacto para reintentarlo.
    if ! gh release view "$TAG" -R "$REPO" >/dev/null 2>&1; then
        echo
        echo "LA RELEASE $TAG NO ESTA PUBLICADA (el tag si). Reintenta con:"
        echo "  gh release create $TAG -R $REPO --title \"35cabina $TAG\" \\"
        echo "     --notes-file $NOTAS_TMP \"$OUT\""
        exit 1
    fi
    # Y ADEMÁS tiene que llevar su .bin: una Release sin asset no la puede
    # descargar nadie, y scripts/grabar.sh la rechazaría (auditoría del
    # 15-oct-2026). GitHub tarda un momento en procesar lo que acaba de subir
    # `gh release create`, así que se hacen 5 comprobaciones con 2 s de espera
    # (unos 10 s en total) antes de darla por incompleta.
    ASSETS=""
    for _ in 1 2 3 4 5; do
        ASSETS="$(gh release view "$TAG" -R "$REPO" --json assets --jq '.assets[].name' 2>/dev/null || true)"
        if printf '%s\n' "$ASSETS" | grep -q '\.bin$'; then break; fi
        sleep 2
    done
    if ! printf '%s\n' "$ASSETS" | grep -q '\.bin$'; then
        echo
        echo "ERROR: el tag $TAG quedó subido, pero la Release está INCOMPLETA: no"
        echo "       tiene ningún asset .bin (visto: ${ASSETS:-ninguno}). El firmware"
        echo "       está en $OUT; súbelo a mano:"
        echo "  gh release upload $TAG -R $REPO \"$OUT\""
        exit 1
    fi
    echo "[ok] Release $TAG publicada con su .bin ($(printf '%s' "$ASSETS" | tr '\n' ' '))"
fi

echo
echo "────────────────────────────────────────────────────────────"
echo "PUBLICADO $TAG. Solo queda grabar:"
echo "  scripts/grabar.sh /dev/ttyACM1     # la 3,5 pulgadas (USB-JTAG). La P4"\
     " es ttyACM0 y la 5 pulgadas ttyUSB0"
echo "────────────────────────────────────────────────────────────"
