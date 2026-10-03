#!/usr/bin/env bash
# auditar.sh — Verificador de la cabina (35cabina). Reglas que evitan que vuelvan
# fallos ya conocidos de ESTE repo (auditoria del 3-oct-2026).
#
# La P4 tiene su propio test/auditar.sh; este es el equivalente aqui, donde hasta
# ahora solo habia compilacion en el CI y los dos chequeos de protocolo.
#
# Uso:  bash test/auditar.sh        (desde la raiz del repo)
set -uo pipefail
cd "$(dirname "$0")/.."
fallos=0
ok()  { printf '  ok    %s\n' "$1"; }
mal() { printf '  FALLO %s\n' "$1"; fallos=$((fallos+1)); }

echo "=== 1. El binario es el de la IDF que pide el proyecto (v5.4.4) ==="
# CLAUDE.md: la IDF de la cabina es v5.4.4 OBLIGATORIA (la P4 va por 5.5.5: son
# proyectos distintos). Compilar con otra deja el firmware sin probar.
BIN=$(ls build/35cabina.bin 2>/dev/null | head -1)
if [ -n "$BIN" ]; then
    strings "$BIN" | grep -qE '^v5\.4\.4$' \
        && ok "el .bin lleva IDF v5.4.4" \
        || mal "el .bin NO lleva IDF v5.4.4 ($(strings "$BIN" | grep -oE '^v5\.[0-9]\.[0-9]+$' | head -1))"
    ver=$(strings "$BIN" | grep -oE '^v2\.[0-9]+$' | head -1)
    [ -n "$ver" ] && ok "version embebida: $ver" || mal "el .bin no lleva version embebida (¿build sin tags?)"
else
    echo "  (sin build: se omite; compila antes con el release.sh del repo)"
fi

echo "=== 2. LVGL 8.4.0 (la UI esta escrita para esa version) ==="
grep -A 6 "^  lvgl/lvgl:" dependencies.lock 2>/dev/null | grep -q "version: 8.4.0" \
    && ok "lvgl/lvgl fijado en 8.4.0" \
    || mal "lvgl/lvgl NO esta fijado en 8.4.0 en dependencies.lock (subir LVGL rompe la UI)"

echo "=== 3. Particion de coredump (para poder diagnosticar un cuelgue) ==="
# Sin esta particion, un panic en el campo no deja volcado y solo se sabe que
# "algo peta". La P4 la tiene por la misma razon.
grep -qE "^coredump" partitions.csv && ok "particion coredump presente" \
    || mal "sin particion coredump: un panic no dejara volcado"

echo "=== 4. Task WDT encendido y con panic (el anti-cuelgue depende de el) ==="
H=build/config/sdkconfig.h
if [ -f "$H" ]; then
    grep -q "^#define CONFIG_ESP_TASK_WDT_EN 1" "$H" && ok "Task WDT encendido" || mal "Task WDT apagado"
    grep -q "^#define CONFIG_ESP_TASK_WDT_PANIC 1" "$H" && ok "Task WDT con panic (reinicia)" || mal "Task WDT sin panic: un cuelgue no reinicia"
fi

echo "=== 5. Pilas de tarea (ninguna por debajo de 3072) ==="
baja=$(grep -rE 'xTaskCreate\([a-zA-Z_0-9]+, *"[a-z_0-9]+", *[0-9]{3,5}' main components --include="*.c" 2>/dev/null \
    | grep -v managed_components | grep -v espressif__ \
    | sed -E 's/^([^:]+):.*"([a-z_0-9]+)", *([0-9]+).*/\3 \2 \1/' \
    | awk '$1+0 < 3072 {print "        " $2 " (" $1 ") en " $3}' | head -5)
[ -z "$baja" ] && ok "ninguna tarea propia con pila < 3072" || { mal "tareas con pila < 3072:"; echo "$baja" | sed 's/^/        /'; }

echo "=== 6. Copias de cadenas sin limite ==="
malas=$(grep -rnE '\b(strcpy|strcat|sprintf)\s*\(' main components --include="*.c" 2>/dev/null \
    | grep -v managed_components | grep -vE '"[^"]*"\)' | head -5)
[ -z "$malas" ] && ok "ninguna copia sin limite con origen variable" || { mal "copias sin limite:"; echo "$malas" | sed 's/^/        /'; }

echo "=== 7. LVGL desde tareas: siempre bajo el cerrojo del port ==="
# Medido a golpes en la P4 el 2-oct-2026: llamar a LVGL desde una tarea que no es
# la de LVGL sin el lock acaba en panic (get_prop_core). Aqui p4_api.c ya lo
# documenta y usa lvgl_port_lock(); esta regla vigila que ningun fichero que
# cree tareas y toque LVGL se deje el cerrojo.
sospechosos=""
for f in $(grep -rlE 'xTaskCreate\(' main components --include="*.c" 2>/dev/null | grep -v managed_components); do
    nlv=$(grep -cE '\blv_[a-z_]+\(' "$f" || true)
    nlock=$(grep -cE 'lvgl_port_lock|bsp_display_lock' "$f" || true)
    # main.c y lv_port.c son la tarea de LVGL y su port: ahi el lock no hace falta
    case "$f" in main/main.c|main/lv_port.c|main/esp_bsp.c) continue;; esac
    [ "$nlv" -gt 0 ] && [ "$nlock" -eq 0 ] && sospechosos="$sospechosos $f"
done
[ -z "$sospechosos" ] && ok "ningun fichero con tareas toca LVGL sin cerrojo" \
    || mal "tocan LVGL sin lvgl_port_lock:$sospechosos"

echo "=== 8. Parametros del enlace con la P4 ==="
# La P4 levanta el AP en 192.168.4.1 y su DHCP reparte .2-.101, asi que la IP
# estatica de la cabina tiene que quedar por encima (se usa .200) o se pelea con
# una concesion y el enlace se cae sin decir por que.
grep -q "192.168.4.1" main/net/p4_api.c && ok "la P4 se busca en 192.168.4.1" \
    || mal "p4_api.c no apunta a 192.168.4.1"
ip=$(grep -oE 'IP_FIJA_CABINA +"192\.168\.4\.[0-9]+"' main/net/udp_rx.c | grep -oE '[0-9]+"$' | tr -d '"')
if [ -n "$ip" ]; then
    [ "$ip" -ge 150 ] && ok "IP estatica de la cabina = .$ip (fuera del DHCP .2-.101)" \
        || mal "IP estatica de la cabina = .$ip: DENTRO del rango DHCP de la P4 (.2-.101)"
else
    mal "no encuentro IP_FIJA_CABINA en udp_rx.c"
fi

echo "=== 9. mini_proto.h y el contrato, contra el repo de la P4 (si esta al lado) ==="
if [ -f ../victron/main/net/mini_proto.h ]; then
    diff -q main/net/mini_proto.h ../victron/main/net/mini_proto.h >/dev/null 2>&1 \
        && ok "mini_proto.h identico al de la P4" \
        || mal "mini_proto.h DISTINTO del de la P4 (se sincroniza a mano)"
    if [ -x ../victron/test/protocolo_cabina.sh ]; then
        if bash ../victron/test/protocolo_cabina.sh ../victron . >/dev/null 2>&1; then
            ok "el contrato completo con la P4 pasa (ops, JSON, campos)"
        else
            mal "el contrato con la P4 FALLA: corre 'bash ../victron/test/protocolo_cabina.sh ../victron .'"
        fi
    fi
else
    echo "  (sin el repo de la P4 al lado: se omite)"
fi

echo "=== 10. CUERPO_MAX tiene que caber en VIAJE_BODY_MAX ==="
# Lo vigila el CI (job viaje_body_sync) pero es barato comprobarlo aqui tambien:
# este repo es el unico que puede romper el limite subiendo CUERPO_MAX.
if [ -f ../victron/main/portal/config_server_viaje.c ]; then
    cmax=$(grep -oE '#define +CUERPO_MAX +[0-9]+' main/net/viaje_cola.h | grep -oE '[0-9]+' | head -1)
    vmax=$(grep -oE '#define +VIAJE_BODY_MAX +[0-9]+' ../victron/main/portal/config_server_viaje.c | grep -oE '[0-9]+' | head -1)
    if [ -n "$cmax" ] && [ -n "$vmax" ]; then
        [ "$cmax" -lt "$vmax" ] && ok "CUERPO_MAX=$cmax < VIAJE_BODY_MAX=$vmax" \
            || mal "CUERPO_MAX=$cmax NO cabe en VIAJE_BODY_MAX=$vmax"
    fi
fi

echo "=== 11. dependencies.lock commiteado, sin rutas absolutas ==="
# Este repo ya tiene un hook de pre-commit para esto (CLAUDE.md); la regla lo
# vuelve a comprobar por si el hook no esta activado en un clon nuevo.
if git show HEAD:dependencies.lock >/dev/null 2>&1; then
    git show HEAD:dependencies.lock | grep -qE "$HOME|/home/" \
        && mal "el dependencies.lock COMMITEADO lleva ruta absoluta: el CI fallara" \
        || ok "el dependencies.lock commiteado solo tiene rutas relativas"
fi

echo "=== 12. El build no puede ser mas viejo que el ultimo commit ==="
if [ -n "$BIN" ]; then
    ultimo=$(git log -1 --format=%ct -- main components partitions.csv sdkconfig 2>/dev/null || echo 0)
    mtime=$(stat -c %Y "$BIN" 2>/dev/null || echo 0)
    if [ "$ultimo" -gt 0 ] && [ "$mtime" -lt "$ultimo" ]; then
        mal "el .bin es MAS VIEJO que el ultimo commit de codigo: recompila antes de auditarlo"
    else
        ok "el .bin es posterior al ultimo commit de codigo"
    fi
fi

echo
if [ "$fallos" -eq 0 ]; then echo "AUDITORIA OK"; exit 0; else echo "AUDITORIA: $fallos FALLOS"; exit 1; fi
