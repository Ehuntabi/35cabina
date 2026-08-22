#!/usr/bin/env python3
"""Markdown -> PDF para el manual de la 35cabina.

Chromium headless es el motor: es lo que hay instalado (no hay pandoc ni
weasyprint) y ademas es el unico que pinta los emoji en color, que aqui no son
adorno -- los circulos de color SON la leyenda del nivel y de la valoracion.
"""
import re
import subprocess
import sys
from pathlib import Path

import markdown

src = Path(sys.argv[1]).resolve()
out = Path(sys.argv[2]).resolve()

texto = src.read_text(encoding="utf-8")

# El primer h1 se convierte en portada, asi que se saca del cuerpo.
lineas = texto.split("\n")
titulo = lineas[0].lstrip("# ").strip()
cuerpo_md = "\n".join(lineas[1:])

html_cuerpo = markdown.markdown(
    cuerpo_md,
    extensions=["tables", "fenced_code", "sane_lists", "attr_list"],
)

# El texto FLUYE: nada de una seccion por pagina. Se probo asi y salian 12 hojas
# con la mitad en blanco -- la seccion 3 ocupaba un tercio de folio. Lo que evita
# los cortes feos no es partir por secciones sino no partir tablas, avisos ni
# maquetas por dentro (page-break-inside abajo), y no dejar un titulo suelto al
# pie (page-break-after: avoid).

CSS = """
@page { size: A4; margin: 18mm 16mm 20mm 16mm; }

:root {
  --azul:   #0d6ea8;
  --tinta:  #1a1a1a;
  --suave:  #5a5a5a;
  --linea:  #d8dee3;
  --fondo:  #f4f7f9;
  --aviso:  #fff4e0;
  --avisob: #e8a33d;
}

* { box-sizing: border-box; }

body {
  font-family: "DejaVu Sans", "Noto Sans", system-ui, sans-serif;
  font-size: 10.5pt;
  line-height: 1.55;
  color: var(--tinta);
  margin: 0;
}

/* --- Portada ----------------------------------------------------------- */
.portada {
  height: 247mm;                 /* A4 menos margenes: fuerza el salto solo */
  display: flex;
  flex-direction: column;
  justify-content: center;
  page-break-after: always;
}
.portada .kicker {
  font-size: 11pt; letter-spacing: .18em; text-transform: uppercase;
  color: var(--azul); font-weight: 700; margin-bottom: 10mm;
}
.portada h1 {
  font-size: 30pt; line-height: 1.15; margin: 0 0 6mm 0;
  color: var(--tinta); border: 0; padding: 0;
}
.portada .sub { font-size: 13pt; color: var(--suave); max-width: 120mm; }
.portada .regla { height: 3px; width: 40mm; background: var(--azul); margin: 8mm 0; }
.portada .pie {
  margin-top: auto; font-size: 9pt; color: var(--suave);
  border-top: 1px solid var(--linea); padding-top: 4mm;
}

/* --- Titulos ----------------------------------------------------------- */
h2 {
  font-size: 16pt; color: var(--azul); margin: 9mm 0 4mm 0;
  padding-bottom: 2mm; border-bottom: 2px solid var(--azul);
}
h2:first-of-type { margin-top: 0; }
h3 {
  font-size: 12.5pt; margin: 7mm 0 2mm 0; color: var(--tinta);
}
h2, h3 { page-break-after: avoid; }

p { margin: 0 0 3.2mm 0; }

/* --- Tablas ------------------------------------------------------------ */
table {
  width: 100%; border-collapse: collapse; margin: 4mm 0 5mm 0;
  font-size: 9.8pt; page-break-inside: avoid;
}
th {
  background: var(--azul); color: #fff; text-align: left;
  padding: 2.2mm 3mm; font-weight: 600;
}
td { padding: 2.2mm 3mm; border-bottom: 1px solid var(--linea); vertical-align: top; }
tr:nth-child(even) td { background: var(--fondo); }

/* --- Avisos (blockquote) ----------------------------------------------- */
blockquote {
  margin: 4mm 0; padding: 3.5mm 5mm;
  background: var(--aviso); border-left: 4px solid var(--avisob);
  page-break-inside: avoid;
}
blockquote p:last-child { margin-bottom: 0; }

/* --- Codigo: aqui son maquetas de pantalla, no codigo ------------------ */
pre {
  background: #12171c; color: #e6edf3; padding: 4mm 5mm;
  border-radius: 2mm; font-size: 9.5pt; line-height: 1.45;
  page-break-inside: avoid; margin: 4mm 0;
}
code { font-family: "DejaVu Sans Mono", monospace; }
p code, li code, td code {
  background: var(--fondo); border: 1px solid var(--linea);
  padding: .3mm 1.2mm; border-radius: 1mm; font-size: 9pt;
}

ul, ol { margin: 0 0 3.5mm 0; padding-left: 6mm; }
li { margin-bottom: 1.4mm; }

hr { display: none; }          /* los --- ya los marca el salto de pagina */

strong { font-weight: 700; }
a { color: var(--azul); text-decoration: none; }
"""

html = f"""<!doctype html>
<html lang="es"><head><meta charset="utf-8">
<title>{titulo}</title><style>{CSS}</style></head>
<body>
<div class="portada">
  <div class="kicker">Autocaravana &middot; Pantalla de cabina</div>
  <h1>{titulo}</h1>
  <div class="regla"></div>
  <div class="sub">Viaje, paradas, repostajes, peajes, bombonas y
  mantenimiento. Escrito para usarlo en la carretera.</div>
  <div class="pie">Version 1.0 &middot; 22 de agosto de 2026</div>
</div>
{html_cuerpo}
</body></html>"""

tmp_html = out.with_suffix(".html")
tmp_html.write_text(html, encoding="utf-8")

subprocess.run(
    ["chromium", "--headless", "--disable-gpu", "--no-sandbox",
     "--no-pdf-header-footer", "--virtual-time-budget=10000",
     f"--print-to-pdf={out}", tmp_html.as_uri()],
    check=True, capture_output=True,
)
tmp_html.unlink()
print(f"{out}  ({out.stat().st_size // 1024} KB)")
