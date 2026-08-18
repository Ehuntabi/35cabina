#ifndef UI_THEME_H
#define UI_THEME_H

#include <stdbool.h>
#include <stdint.h>
#include "lvgl.h"

/* ============================================================================
 * Tema visual unificado para la 3.5" (QSPI AXS15231B). Panel nativo
 * 320x480, pero con la rotacion de 90 grados que aplica main.c la
 * resolucion LOGICA que ve LVGL es 480x320 LANDSCAPE (ver
 * esp_bsp.c:390-396: intercambia hres/vres cuando rotate != NONE/180).
 * Este comentario decia "320x480 portrait" y estaba mal -- corregido
 * 18-ago-2026 tras verificarlo contra el codigo real.
 *
 * Estilo card-based: cada panel de información es una "card" con fondo oscuro
 * con gradiente sutil, borde fino del color que identifica su rol (batería
 * naranja, solar verde, DC/DC cyan, etc.) y radios redondeados.
 *
 * Uso típico desde una vista Live:
 *
 *   lv_obj_t *card = ui_theme_card_create(parent, UI_ROLE_BATTERY,
 *                                          "BATERIA", LV_SYMBOL_BATTERY_FULL);
 *   // dentro del card colocas tus labels normalmente con lv_obj_align
 *   // contra el card (la cabecera ya está creada por el helper).
 * ============================================================================ */

/* --- Paleta semántica (rol → color borde + acento) --- */
#define UI_COL_BG           lv_color_hex(0x000000)
#define UI_COL_CARD_TOP     lv_color_hex(0x0A0A0A)
#define UI_COL_CARD_BOT     lv_color_hex(0x161616)
#define UI_COL_TEXT         lv_color_hex(0xFFFFFF)
#define UI_COL_TEXT_DIM     lv_color_hex(0x888888)
#define UI_COL_SEP_BG       lv_color_hex(0x282828)

/* --- Estado: codifica magnitud cualitativa del valor (igual que en mini) --- */
#define UI_COL_GOOD         lv_color_hex(0x4CD964)   /* verde suave */
#define UI_COL_WARN         lv_color_hex(0xFFD54F)   /* amarillo */
#define UI_COL_BAD          lv_color_hex(0xFF4444)   /* rojo */

/* --- Roles (color de borde por tipo de card) --- */
typedef enum {
    UI_ROLE_BATTERY,        /* naranja  — SmartShunt / BMV */
    UI_ROLE_SOLAR,          /* verde    — MPPT */
    UI_ROLE_DCDC,           /* cyan     — Orion XS u otros */
    UI_ROLE_INVERTER,       /* morado   — Inverter */
    UI_ROLE_FRIDGE,         /* azul     — Frigo */
    UI_ROLE_TEMP_EXT,       /* amarillo — Temperatura exterior */
    UI_ROLE_NEUTRAL,        /* gris     — Cards genéricas / Settings */
} ui_role_t;

lv_color_t ui_theme_role_color(ui_role_t role);

/* --- Card factory ---
 * Crea una card 100 % del ancho del padre por defecto. Después puedes
 * redimensionarla con lv_obj_set_size. Si `title` no es NULL, añade
 * cabecera con icono (opcional, NULL = solo texto) y separador color rol. */
lv_obj_t *ui_theme_card_create(lv_obj_t *parent,
                               ui_role_t role,
                               const char *title,
                               const char *icon_utf8);

/* --- Helpers de color según valor --- */
lv_color_t ui_theme_color_for_soc(int16_t soc_deci);
lv_color_t ui_theme_color_for_fridge_temp(int16_t centi);

/* --- Pulse animado del borde (alarma activa) --- */
void ui_theme_pulse_start(lv_obj_t *card);
void ui_theme_pulse_stop(lv_obj_t *card);

/* --- Indicador conexión: punto pequeño cuyo color refleja "fresco/stale" ---
 * Llamar `ui_theme_conn_dot_create` una vez al crear UI, guardar el handle,
 * y luego llamar `ui_theme_conn_dot_mark_fresh` cada vez que llegue un
 * paquete BLE / dato. Si no llega ningún paquete en `stale_after_ms`,
 * el dot pasa a rojo automáticamente. */
lv_obj_t *ui_theme_conn_dot_create(lv_obj_t *parent);
void ui_theme_conn_dot_mark_fresh(lv_obj_t *dot);
void ui_theme_conn_dot_set_threshold(uint32_t stale_after_ms);

#endif /* UI_THEME_H */
