#!/usr/bin/env bash
#
# release.sh — publica una versión de la 3,5" de cabina.
#
# Uso:  ./release.sh X.Y.Z  ["mensaje del tag"]
#   ej: ./release.sh 1.9.8 "El icono del GPS ya no cuelga del shunt"
#
# Qué hace:
#   1. Exige el árbol limpio (lo que se publica tiene que ser lo que hay).
#   2. Crea el tag vX.Y.Z, o reutiliza el que ya esté EN ESTE MISMO commit.
#   3. Reconfigura y compila -- el reconfigure no es opcional: la versión se
#      calcula al CONFIGURAR, no al compilar, así que sin él se publicaría un
#      binario con la versión anterior dentro.
#   4. VERIFICA que la versión embebida en el .bin coincide con el tag.
#   5. Sube código y tag, y publica la Release en GitHub con el binario.
#
# Existe porque este proyecto no tenía ninguno: 20 etiquetas creadas y CERO
# Releases publicadas. Y en el proyecto hermano, que sí lo tenía, el script
# solo IMPRIMÍA los comandos de publicar y había que copiarlos a mano: acabó
# con 74 tags y la última Release en la v1.5.5.
set -euo pipefail
cd "$(dirname "$0")"

REPO="Ehuntabi/35cabina"
IDF_EXPORT="$HOME/.espressif/esp-idf-5.4/export.sh"
RELDIR="$HOME/joint-releases"
APP_BIN="build/35cabina.bin"

[ $# -ge 1 ] || { echo "Uso: ./release.sh X.Y.Z [\"mensaje\"]"; exit 1; }
VER="$1"; TAG="v$VER"
MSG="${2:-Release $TAG}"

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
    echo "AVISO: no está 'gh': la Release NO se ha publicado."
    exit 0
fi
if gh release view "$TAG" -R "$REPO" >/dev/null 2>&1; then
    echo "[ok] la Release $TAG ya existe, no la toco"
else
    git tag -l --format='%(contents)' "$TAG" > "/tmp/notas-35cabina-$TAG.md"
    gh release create "$TAG" -R "$REPO" \
        --title "35cabina $TAG" \
        --notes-file "/tmp/notas-35cabina-$TAG.md" \
        "$OUT"
    echo "[ok] Release $TAG publicada"
fi

echo
echo "────────────────────────────────────────────────────────────"
echo "PUBLICADO $TAG. Solo queda grabar:"
echo "  idf.py -p /dev/ttyACM0 flash"
echo "────────────────────────────────────────────────────────────"
