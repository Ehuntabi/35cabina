/* view_info.c - Pantalla de info agrupada (Fase 1).
 *
 * Patron de colores/umbrales y LEDs de agua portado de
 * ~/joint/victron_mini/main/ui/view_quad.c, reescrito como grid (todas las
 * cards visibles a la vez, sin rotacion) porque aqui hay mucho mas sitio
 * que en la pantalla de 320x172 del mini.
 */
#include "view_info.h"
#include "../data_model.h"
#include "esp_timer.h"
#include <stdio.h>

#define COL_CARD_BG_TOP  lv_color_hex(0x0A0A0A)
#define COL_CARD_BG_BOT  lv_color_hex(0x161616)
#define COL_TEXT         lv_color_hex(0xFFFFFF)
#define COL_TEXT_DIM     lv_color_hex(0x888888)
#define COL_VAL_GOOD     lv_color_hex(0x4CD964)
#define COL_VAL_WARN     lv_color_hex(0xFFD54F)
#define COL_VAL_BAD      lv_color_hex(0xFF4444)
#define COL_CONN_NONE    lv_color_hex(0x555555)
#define COL_CONN_OK      lv_color_hex(0x00C851)
#define COL_CONN_LOST    lv_color_hex(0xFF4444)
#define CONN_TIMEOUT_MS  5000

#define COL_BORDER_BAT   lv_color_hex(0xFF9800)  /* SmartShunt naranja */
#define COL_BORDER_AUX   lv_color_hex(0x4FC3F7)  /* Bateria motor cyan */
#define COL_BORDER_DCDC  lv_color_hex(0xAB47BC)  /* DC/DC morado (nueva) */
#define COL_BORDER_COLD  lv_color_hex(0x66CCFF)  /* Frigo azul claro */
#define COL_BORDER_WATER lv_color_hex(0x29B6F6)  /* Aguas azul saturado */
#define COL_BORDER_HEAT  lv_color_hex(0xFFD54F)  /* Exterior amarillo calido */

typedef struct {
    lv_obj_t *card;
    lv_obj_t *primary;
    lv_obj_t *secondary;
    lv_obj_t *conn_dot;
} info_cell_t;

static info_cell_t s_bat, s_aux, s_dcdc, s_frigo, s_ext;
static lv_obj_t   *s_water_card;
static lv_obj_t   *s_water_conn_dot;
static lv_obj_t   *s_led_clean[4];
static lv_obj_t   *s_led_gray;
static lv_timer_t *s_refresh_timer;

static lv_color_t color_for_soc(int16_t soc_deci) {
    if (soc_deci >= 600) return COL_VAL_GOOD;
    if (soc_deci >= 300) return COL_VAL_WARN;
    return COL_VAL_BAD;
}

static lv_color_t color_for_frigo(int16_t centi) {
    if (centi <= -1500) return COL_VAL_GOOD;  /* <= -15C ok */
    if (centi <= -500)  return COL_VAL_WARN;  /* -15..-5 aviso */
    return COL_VAL_BAD;                        /* > -5 mal */
}

/* VIC_STATE_* de victron_records.h (proyecto P4) */
static const char *dcdc_state_name(uint8_t state) {
    switch (state) {
        case 0x00: return "Apagado";
        case 0x01: return "Bajo consumo";
        case 0x02: return "Fallo";
        case 0x03: return "Carga";
        case 0x04: return "Absorcion";
        case 0x05: return "Flotacion";
        case 0x06: return "Almacenaje";
        case 0x07: return "Ecualizacion";
        case 0x0B: return "Fuente";
        default:   return "--";
    }
}

static lv_obj_t *make_cell(lv_obj_t *grid, lv_color_t border, const char *title_text,
                            uint8_t col, uint8_t row, info_cell_t *out)
{
    lv_obj_t *card = lv_obj_create(grid);
    lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_bg_color(card, COL_CARD_BG_TOP, 0);
    lv_obj_set_style_bg_grad_color(card, COL_CARD_BG_BOT, 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, border, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 4, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dot = lv_obj_create(card);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_bg_color(dot, COL_CONN_NONE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, 4, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 2, 2);
    if (out) out->conn_dot = dot;

    lv_obj_t *t = lv_label_create(card);
    lv_label_set_text(t, title_text);
    lv_obj_set_style_text_color(t, border, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *p = lv_label_create(card);
    lv_label_set_text(p, "--");
    lv_obj_set_style_text_color(p, COL_TEXT, 0);
    lv_obj_set_style_text_font(p, &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_align(p, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(p, LV_ALIGN_CENTER, 0, 2);
    if (out) out->primary = p;

    lv_obj_t *s = lv_label_create(card);
    lv_label_set_text(s, "");
    lv_obj_set_style_text_color(s, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(s, LV_ALIGN_BOTTOM_MID, 0, 0);
    if (out) out->secondary = s;

    if (out) out->card = card;
    return card;
}

/* === LEDs de aguas (mismo patron que view_quad.c del mini) ============= */
static void led_blink_cb(void *var, int32_t v)
{
    lv_obj_set_style_bg_opa((lv_obj_t *)var, v, 0);
    lv_obj_set_style_border_opa((lv_obj_t *)var, v, 0);
}

/* mode: 0 = apagado - 1 = lleno (azul) - 2 = alarma (rojo parpadeando) */
static void led_set(lv_obj_t *led, uint8_t mode)
{
    lv_anim_del(led, led_blink_cb);
    if (mode == 2) {
        lv_obj_set_style_bg_color(led, COL_VAL_BAD, 0);
        lv_obj_set_style_border_color(led, COL_VAL_BAD, 0);
        lv_obj_set_style_bg_opa(led, LV_OPA_COVER, 0);
        lv_anim_t a;
        lv_anim_init(&a);
        lv_anim_set_var(&a, led);
        lv_anim_set_exec_cb(&a, led_blink_cb);
        lv_anim_set_values(&a, 255, 60);
        lv_anim_set_time(&a, 400);
        lv_anim_set_playback_time(&a, 400);
        lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
        lv_anim_start(&a);
    } else {
        lv_obj_set_style_bg_opa(led, LV_OPA_COVER, 0);
        lv_obj_set_style_border_opa(led, LV_OPA_COVER, 0);
        if (mode == 1) {
            lv_obj_set_style_bg_color(led, lv_color_hex(0x4FC3F7), 0);
            lv_obj_set_style_border_color(led, lv_color_hex(0x4FC3F7), 0);
        } else {
            lv_obj_set_style_bg_color(led, lv_color_hex(0x141414), 0);
            lv_obj_set_style_border_color(led, lv_color_hex(0x333333), 0);
        }
    }
}

static lv_obj_t *make_led(lv_obj_t *parent, bool round)
{
    lv_obj_t *led = lv_obj_create(parent);
    lv_obj_set_size(led, 16, 16);
    lv_obj_set_style_radius(led, round ? LV_RADIUS_CIRCLE : 4, 0);
    lv_obj_set_style_border_width(led, 2, 0);
    lv_obj_set_style_pad_all(led, 0, 0);
    lv_obj_clear_flag(led, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    led_set(led, 0);
    return led;
}

static void make_water_cell(lv_obj_t *grid, uint8_t col, uint8_t row)
{
    lv_obj_t *card = lv_obj_create(grid);
    lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, 1, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_bg_color(card, COL_CARD_BG_TOP, 0);
    lv_obj_set_style_bg_grad_color(card, COL_CARD_BG_BOT, 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, COL_BORDER_WATER, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 4, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);
    s_water_card = card;

    lv_obj_t *dot = lv_obj_create(card);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_bg_color(dot, COL_CONN_NONE, 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, 4, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(dot, LV_ALIGN_TOP_LEFT, 2, 2);
    s_water_conn_dot = dot;

    lv_obj_t *t = lv_label_create(card);
    lv_label_set_text(t, "AGUAS");
    lv_obj_set_style_text_color(t, COL_BORDER_WATER, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);

    lv_obj_t *row_cont = lv_obj_create(card);
    lv_obj_set_size(row_cont, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(row_cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row_cont, 0, 0);
    lv_obj_set_style_pad_all(row_cont, 0, 0);
    lv_obj_set_style_pad_column(row_cont, 6, 0);
    lv_obj_set_flex_flow(row_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(row_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(row_cont, LV_ALIGN_CENTER, 0, 2);

    for (int i = 0; i < 4; i++) {
        s_led_clean[i] = make_led(row_cont, false);
    }
    s_led_gray = make_led(row_cont, true);   /* redondo: distingue grises de limpia */

    lv_obj_t *s = lv_label_create(card);
    lv_label_set_text(s, "limpia (4) / grises");
    lv_obj_set_style_text_color(s, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
    lv_obj_align(s, LV_ALIGN_BOTTOM_MID, 0, 0);
}

/* === Refresco =========================================================== */

static void refresh_bat(const mini_data_t *d)
{
    char buf[32];
    if (d->has_data) {
        lv_color_t soc_col = color_for_soc(d->shunt_soc_deci);
        snprintf(buf, sizeof(buf), "%d%%", d->shunt_soc_deci / 10);
        lv_label_set_text(s_bat.primary, buf);
        lv_obj_set_style_text_color(s_bat.primary, soc_col, 0);

        int32_t ma = d->shunt_current_milli;
        char sgn = ma < 0 ? '-' : '+';
        int32_t am = ma < 0 ? -ma : ma;
        snprintf(buf, sizeof(buf), "%d.%02dV  %c%ld.%ldA",
                 d->shunt_voltage_centi / 100, d->shunt_voltage_centi % 100,
                 sgn, (long)(am / 1000), (long)((am % 1000) / 100));
        lv_label_set_text(s_bat.secondary, buf);
    } else {
        lv_label_set_text(s_bat.primary, "--");
        lv_obj_set_style_text_color(s_bat.primary, COL_TEXT, 0);
        lv_label_set_text(s_bat.secondary, "sin datos");
    }
}

/* Mismo criterio de formato que ui_format_aux_value() en la P4:
 * aux_value_raw crudo, la unidad depende de aux_input (0/1=V*100, 2=Kelvin*100). */
static void refresh_aux(const mini_data_t *d)
{
    char buf[32];
    if (d->aux_has_data) {
        const char *mode_text;
        if (d->aux_input == 2) {
            int temp_centi = (int)d->aux_value_raw - 27315;
            int ti = temp_centi / 100;
            int td = (temp_centi >= 0 ? temp_centi : -temp_centi) % 100 / 10;
            snprintf(buf, sizeof(buf), "%d.%dC", ti, td);
            mode_text = "temperatura";
        } else {
            snprintf(buf, sizeof(buf), "%d.%02dV",
                     d->aux_value_raw / 100, d->aux_value_raw % 100);
            mode_text = (d->aux_input == 1) ? "punto medio" : "arranque";
        }
        lv_label_set_text(s_aux.primary, buf);
        lv_obj_set_style_text_color(s_aux.primary, COL_TEXT, 0);
        lv_label_set_text(s_aux.secondary, mode_text);
    } else {
        lv_label_set_text(s_aux.primary, "--");
        lv_label_set_text(s_aux.secondary, "sin datos");
    }
}

static void refresh_dcdc(const mini_data_t *d)
{
    char buf[32];
    if (d->dcdc_has_data) {
        snprintf(buf, sizeof(buf), "%d.%02dV",
                 d->dcdc_v_out_centi / 100, d->dcdc_v_out_centi % 100);
        lv_label_set_text(s_dcdc.primary, buf);
        lv_obj_set_style_text_color(s_dcdc.primary, COL_TEXT, 0);
        lv_label_set_text(s_dcdc.secondary, dcdc_state_name(d->dcdc_state));
    } else {
        lv_label_set_text(s_dcdc.primary, "--");
        lv_label_set_text(s_dcdc.secondary, "sin datos");
    }
}

static void refresh_temp(info_cell_t *cell, bool has, int16_t centi, bool is_frigo,
                          const char *extra_secondary)
{
    char buf[24];
    if (has) {
        int sign = centi < 0 ? -1 : 1;
        int abs_c = centi * sign;
        snprintf(buf, sizeof(buf), "%s%d.%d\xC2\xB0" "C",
                 sign < 0 ? "-" : "", abs_c / 100, (abs_c % 100) / 10);
        lv_label_set_text(cell->primary, buf);
        lv_obj_set_style_text_color(cell->primary, is_frigo ? color_for_frigo(centi) : COL_TEXT, 0);
        lv_label_set_text(cell->secondary, extra_secondary ? extra_secondary : "");
    } else {
        lv_label_set_text(cell->primary, "--");
        lv_obj_set_style_text_color(cell->primary, COL_TEXT, 0);
        lv_label_set_text(cell->secondary, "sin sonda");
    }
}

static void refresh_aguas(const mini_data_t *d)
{
    if (d->water_has_data) {
        uint8_t cl = d->water_clean > 4 ? 4 : d->water_clean;
        for (int i = 0; i < 4; i++) {
            uint8_t mode = (cl == 0) ? 2 : (i < cl ? 1 : 0);
            led_set(s_led_clean[i], mode);
        }
        led_set(s_led_gray, d->water_gray > 0 ? 2 : 0);
    } else {
        for (int i = 0; i < 4; i++) led_set(s_led_clean[i], 0);
        led_set(s_led_gray, 0);
    }
}

static void update_conn_dots(const mini_data_t *d)
{
    /* Un unico frame UDP 1Hz trae todo el paquete -> mismo estado de
     * enlace para todas las cards (igual que en el mini). */
    lv_color_t col;
    if (!d->has_data) {
        col = COL_CONN_NONE;
    } else {
        uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);
        col = (now - d->last_update_ms < CONN_TIMEOUT_MS) ? COL_CONN_OK : COL_CONN_LOST;
    }
    lv_obj_set_style_bg_color(s_bat.conn_dot, col, 0);
    lv_obj_set_style_bg_color(s_aux.conn_dot, col, 0);
    lv_obj_set_style_bg_color(s_dcdc.conn_dot, col, 0);
    lv_obj_set_style_bg_color(s_frigo.conn_dot, col, 0);
    lv_obj_set_style_bg_color(s_ext.conn_dot, col, 0);
    lv_obj_set_style_bg_color(s_water_conn_dot, col, 0);
}

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    mini_data_t d;
    data_model_get(&d);

    char fan_buf[16];
    const char *frigo_secondary = "";
    if (d.frigo_has_data) {
        snprintf(fan_buf, sizeof(fan_buf), "vent. %u%%", d.frigo_fan_pct);
        frigo_secondary = fan_buf;
    }

    refresh_bat(&d);
    refresh_aux(&d);
    refresh_dcdc(&d);
    refresh_temp(&s_frigo, d.frigo_has_data, d.frigo_temp_centi, true, frigo_secondary);
    refresh_temp(&s_ext, d.exterior_has_data, d.exterior_temp_centi, false, "");
    refresh_aguas(&d);
    update_conn_dots(&d);
}

void view_info_create(lv_obj_t *parent)
{
    /* Pantalla 320x480 PORTRAIT (ver ui_theme.h) -> 2 columnas x 3 filas,
     * no 3x2: con solo 320px de ancho, 3 columnas dejarian cada card en
     * ~104px, demasiado estrecho para "BATERIA MOTOR" + valores. */
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_set_size(grid, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(grid, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 4, 0);
    lv_obj_set_style_pad_gap(grid, 4, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);
    lv_obj_center(grid);

    make_cell(grid, COL_BORDER_BAT,  "BATERIA",        0, 0, &s_bat);
    make_cell(grid, COL_BORDER_AUX,  "BATERIA MOTOR",  1, 0, &s_aux);
    make_cell(grid, COL_BORDER_DCDC, "DC/DC",          0, 1, &s_dcdc);
    make_cell(grid, COL_BORDER_COLD, "FRIGO",          1, 1, &s_frigo);
    make_water_cell(grid,                              0, 2);
    make_cell(grid, COL_BORDER_HEAT, "EXTERIOR",       1, 2, &s_ext);

    s_refresh_timer = lv_timer_create(refresh_cb, 500, NULL);
}
