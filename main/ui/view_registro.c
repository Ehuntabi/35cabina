/* view_registro.c - Menu de registros con iconos (Fase 2, ampliado).
 *
 * Rejilla de 5 categorias (iconos del set built-in de LVGL, sin assets de
 * imagen). Cada una abre su formulario a pantalla completa con boton de
 * volver. Reutiliza el patron de campo+teclado ya validado en la version
 * anterior (solo repostaje/bombona).
 *
 * Nada de esto envia datos todavia -- todos los "Guardar" (y los botones
 * de Iniciar/Finalizar viaje) solo loguean. El envio real a la P4
 * (mini_cmd_t nuevo + receptor en ~/joint/victron) es la Fase 4,
 * deliberadamente fuera de este repo. Inicio/fin de viaje es el primer
 * candidato cuando se abra esa fase (pedido explicito del usuario: "que
 * mande el de 3.5 porque es mas comodo").
 */
#include "view_registro.h"
#include "esp_log.h"
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "view_registro";

typedef enum {
    CAT_VIAJE = 0,
    CAT_REPOSTAJE,
    CAT_PEAJE,
    CAT_BOMBONA,
    CAT_MANTENIMIENTO,
    CAT_COUNT
} categoria_t;

static lv_obj_t *s_keyboard;
static lv_obj_t *s_grid;
static lv_obj_t *s_forms[CAT_COUNT];

static lv_obj_t *s_repo_importe_ta;
static lv_obj_t *s_repo_litros_ta;
static lv_obj_t *s_repo_currency_dd;
static lv_obj_t *s_repo_preciolitro_lbl;

/* Monedas de la Europa continental que puede pisar la autocaravana.
 * Por defecto EUR (indice 0). Mismo orden en las dos listas. */
#define CURRENCY_OPTIONS \
    "EUR \xE2\x82\xAC\n" "GBP \xC2\xA3\n" "CHF Fr\n" "SEK kr\n" \
    "NOK kr\n" "DKK kr\n" "PLN z\xC5\x82\n" "CZK K\xC4\x8D\n" \
    "HUF Ft\n" "RON lei"
static const char *const CURRENCY_CODES[] = {
    "EUR", "GBP", "CHF", "SEK", "NOK", "DKK", "PLN", "CZK", "HUF", "RON"
};

/* === Navegacion grid <-> formulario ===================================== */

static void show_grid(void)
{
    lv_obj_clear_flag(s_grid, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < CAT_COUNT; i++) {
        lv_obj_add_flag(s_forms[i], LV_OBJ_FLAG_HIDDEN);
    }
}

static void show_form(int idx)
{
    lv_obj_add_flag(s_grid, LV_OBJ_FLAG_HIDDEN);
    for (int i = 0; i < CAT_COUNT; i++) {
        if (i == idx) {
            lv_obj_clear_flag(s_forms[i], LV_OBJ_FLAG_HIDDEN);
        } else {
            lv_obj_add_flag(s_forms[i], LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void icon_click_cb(lv_event_t *e)
{
    int idx = (int)(uintptr_t)lv_event_get_user_data(e);
    show_form(idx);
}

static void back_click_cb(lv_event_t *e)
{
    (void)e;
    show_grid();
}

/* === Teclado en pantalla compartido (mismo patron que antes) ============ */

static void ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    bool numeric = (bool)(uintptr_t)lv_event_get_user_data(e);
    lv_keyboard_set_textarea(s_keyboard, ta);
    lv_keyboard_set_mode(s_keyboard, numeric ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_keyboard);
}

static void ta_defocus_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static void kb_close_cb(lv_event_t *e)
{
    (void)e;
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
}

static lv_obj_t *make_field(lv_obj_t *parent, const char *label_text,
                             const char *placeholder, bool numeric)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), 52);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 2, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *ta = lv_textarea_create(cont);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_set_size(ta, lv_pct(100), 30);
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 0, 18);
    if (numeric) {
        lv_textarea_set_accepted_chars(ta, "0123456789.");
    }
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_FOCUSED, (void *)(uintptr_t)numeric);
    lv_obj_add_event_cb(ta, ta_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    return ta;
}

/* Campo de importe: numero + selector de moneda (por defecto EUR, pero
 * contempla otras monedas de la Europa continental para cuando se viaje
 * fuera de la zona euro). Devuelve la textarea; *dd_out (si no es NULL)
 * se rellena con el dropdown de moneda por si hace falta leerlo luego. */
static lv_obj_t *make_money_field(lv_obj_t *parent, const char *label_text,
                                   lv_obj_t **dd_out)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), 52);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 2, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_TOP_LEFT, 0, 0);

    lv_obj_t *ta = lv_textarea_create(cont);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, "0.00");
    lv_obj_set_size(ta, lv_pct(62), 30);
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 0, 18);
    lv_textarea_set_accepted_chars(ta, "0123456789.");
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_FOCUSED, (void *)(uintptr_t)true);
    lv_obj_add_event_cb(ta, ta_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    lv_obj_t *dd = lv_dropdown_create(cont);
    lv_dropdown_set_options(dd, CURRENCY_OPTIONS);
    lv_obj_set_size(dd, lv_pct(36), 30);
    lv_obj_align(dd, LV_ALIGN_TOP_RIGHT, 0, 18);
    if (dd_out) *dd_out = dd;

    return ta;
}

static lv_obj_t *make_readonly_row(lv_obj_t *parent, const char *label_text)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), 34);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 2, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *val = lv_label_create(cont);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_14, 0);
    lv_obj_align(val, LV_ALIGN_RIGHT_MID, 0, 0);

    return val;
}

static lv_obj_t *make_form_container(lv_obj_t *parent)
{
    lv_obj_t *form = lv_obj_create(parent);
    lv_obj_set_size(form, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(form, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(form, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(form, 0, 0);
    lv_obj_set_style_pad_all(form, 8, 0);
    lv_obj_set_style_pad_row(form, 4, 0);
    lv_obj_set_flex_flow(form, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(form, LV_OBJ_FLAG_HIDDEN);
    return form;
}

static void add_header(lv_obj_t *form, const char *title, lv_color_t color)
{
    lv_obj_t *row = lv_obj_create(form);
    lv_obj_set_size(row, lv_pct(100), 34);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_btn_create(row);
    lv_obj_set_size(back, 80, 32);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x333333), 0);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back, back_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Volver");
    lv_obj_center(back_lbl);

    lv_obj_t *t = lv_label_create(row);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, color, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_22, 0);
    lv_obj_align(t, LV_ALIGN_RIGHT_MID, 0, 0);
}

/* === Callbacks de guardado (solo log, ver Fase 4) ======================= */

static void save_generic_cb(lv_event_t *e)
{
    const char *name = (const char *)lv_event_get_user_data(e);
    ESP_LOGI(TAG, "Guardar '%s' pulsado -- TODO Fase 4: enviar por mini_cmd_t a la P4", name);
}

static void viaje_iniciar_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Iniciar viaje pulsado -- TODO Fase 4: comando a trip_manager.c de la P4");
}

static void viaje_finalizar_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Finalizar viaje pulsado -- TODO Fase 4: comando a trip_manager.c de la P4");
}

static lv_obj_t *make_save_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb, void *user_data)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E7D32), 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    return btn;
}

/* === Repostaje: precio/litro calculado en vivo =========================== */

static void repo_recalc_cb(lv_event_t *e)
{
    (void)e;
    float importe = atof(lv_textarea_get_text(s_repo_importe_ta));
    float litros = atof(lv_textarea_get_text(s_repo_litros_ta));
    uint16_t cur_idx = lv_dropdown_get_selected(s_repo_currency_dd);
    const char *cur = cur_idx < (sizeof(CURRENCY_CODES) / sizeof(CURRENCY_CODES[0]))
                       ? CURRENCY_CODES[cur_idx] : "EUR";
    char buf[24];
    if (litros > 0.0f) {
        snprintf(buf, sizeof(buf), "%.3f %s/L", importe / litros, cur);
    } else {
        snprintf(buf, sizeof(buf), "--");
    }
    lv_label_set_text(s_repo_preciolitro_lbl, buf);
}

/* === Construccion de cada formulario ===================================== */

static void build_viaje(lv_obj_t *form)
{
    add_header(form, "VIAJE", lv_color_hex(0xFF9800));

    /* Sin campo de coordenada GPS manual aqui a proposito: cuando exista
     * el GPS real (Fase 4 + modulo GPS de la P4) la posicion de
     * inicio/fin se capturaria sola en el instante del toque, no tiene
     * sentido pedirla a mano para una accion pensada como "un solo toque". */
    lv_obj_t *msg = lv_label_create(form);
    lv_label_set_text(msg, "Controla el inicio/fin de viaje de la P4\ndesde aqui (mas comodo que el 7\").");
    lv_obj_set_style_text_color(msg, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(msg, lv_pct(100));

    lv_obj_t *btn_ini = lv_btn_create(form);
    lv_obj_set_width(btn_ini, lv_pct(100));
    lv_obj_set_style_bg_color(btn_ini, lv_color_hex(0x2E7D32), 0);
    lv_obj_add_event_cb(btn_ini, viaje_iniciar_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l1 = lv_label_create(btn_ini);
    lv_label_set_text(l1, LV_SYMBOL_PLAY "  Iniciar viaje");
    lv_obj_center(l1);

    lv_obj_t *btn_fin = lv_btn_create(form);
    lv_obj_set_width(btn_fin, lv_pct(100));
    lv_obj_set_style_bg_color(btn_fin, lv_color_hex(0xB71C1C), 0);
    lv_obj_add_event_cb(btn_fin, viaje_finalizar_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *l2 = lv_label_create(btn_fin);
    lv_label_set_text(l2, LV_SYMBOL_STOP "  Finalizar viaje");
    lv_obj_center(l2);
}

static void build_repostaje(lv_obj_t *form)
{
    add_header(form, "REPOSTAJE", lv_color_hex(0xFF9800));

    make_field(form, "Coordenada GPS", "40.4168,-3.7038", false);
    make_field(form, "Hora (HH:MM)", "12:00", false);
    s_repo_importe_ta = make_money_field(form, "Importe", &s_repo_currency_dd);
    s_repo_litros_ta  = make_field(form, "Litros", "0.0", true);
    lv_obj_add_event_cb(s_repo_importe_ta, repo_recalc_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_repo_litros_ta, repo_recalc_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_repo_currency_dd, repo_recalc_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_repo_preciolitro_lbl = make_readonly_row(form, "Precio/litro (calculado)");

    make_save_button(form, "Guardar repostaje", save_generic_cb, "repostaje");
}

static void build_peaje(lv_obj_t *form)
{
    add_header(form, "PEAJE", lv_color_hex(0xFF9800));

    make_field(form, "Coordenada GPS", "40.4168,-3.7038", false);
    make_field(form, "Hora (HH:MM)", "12:00", false);
    make_money_field(form, "Importe", NULL);

    make_save_button(form, "Guardar peaje", save_generic_cb, "peaje");
}

static void build_bombona(lv_obj_t *form)
{
    add_header(form, "CAMBIO DE BOMBONA", lv_color_hex(0xFF9800));

    make_field(form, "Coordenada GPS", "40.4168,-3.7038", false);
    make_field(form, "Dia (DD/MM/AAAA)", "18/08/2026", false);
    make_field(form, "Hora (HH:MM)", "12:00", false);
    make_field(form, "Lugar", "", false);

    make_save_button(form, "Guardar cambio de bombona", save_generic_cb, "bombona");
}

static void build_mantenimiento(lv_obj_t *form)
{
    add_header(form, "MANTENIMIENTO", lv_color_hex(0xFF9800));

    make_field(form, "Coordenada GPS", "40.4168,-3.7038", false);

    lv_obj_t *lbl = lv_label_create(form);
    lv_label_set_text(lbl, "Tipo");
    lv_obj_set_style_text_color(lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);

    lv_obj_t *dd = lv_dropdown_create(form);
    lv_dropdown_set_options(dd, "Aceite\nFiltro de aceite\nCorrea\nRuedas");
    lv_obj_set_width(dd, lv_pct(100));

    make_field(form, "Km", "0", true);

    make_save_button(form, "Guardar mantenimiento", save_generic_cb, "mantenimiento");
}

/* === Grid principal ======================================================= */

static lv_obj_t *make_icon_button(lv_obj_t *parent, const char *symbol,
                                   const char *label_text, categoria_t idx)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, 82, 82);
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x1A1A1A), 0);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(btn, icon_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);

    lv_obj_t *ic = lv_label_create(btn);
    lv_label_set_text(ic, symbol);
    lv_obj_set_style_text_color(ic, lv_color_hex(0xFF9800), 0);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_22, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);

    return btn;
}

void view_registro_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* --- Grid de iconos --- */
    s_grid = lv_obj_create(parent);
    lv_obj_set_size(s_grid, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(s_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_grid, 0, 0);
    lv_obj_clear_flag(s_grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(s_grid, 8, 0);
    lv_obj_set_style_pad_gap(s_grid, 10, 0);

    make_icon_button(s_grid, LV_SYMBOL_GPS,      "Viaje",         CAT_VIAJE);
    make_icon_button(s_grid, LV_SYMBOL_TINT,     "Repostaje",     CAT_REPOSTAJE);
    make_icon_button(s_grid, LV_SYMBOL_LIST,     "Peaje",         CAT_PEAJE);
    make_icon_button(s_grid, LV_SYMBOL_REFRESH,  "Bombona",       CAT_BOMBONA);
    make_icon_button(s_grid, LV_SYMBOL_SETTINGS, "Mantenimiento", CAT_MANTENIMIENTO);

    /* --- Formularios (ocultos hasta que se elige un icono) --- */
    s_forms[CAT_VIAJE] = make_form_container(parent);
    build_viaje(s_forms[CAT_VIAJE]);

    s_forms[CAT_REPOSTAJE] = make_form_container(parent);
    build_repostaje(s_forms[CAT_REPOSTAJE]);

    s_forms[CAT_PEAJE] = make_form_container(parent);
    build_peaje(s_forms[CAT_PEAJE]);

    s_forms[CAT_BOMBONA] = make_form_container(parent);
    build_bombona(s_forms[CAT_BOMBONA]);

    s_forms[CAT_MANTENIMIENTO] = make_form_container(parent);
    build_mantenimiento(s_forms[CAT_MANTENIMIENTO]);

    /* --- Teclado compartido --- */
    s_keyboard = lv_keyboard_create(parent);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_keyboard, kb_close_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_keyboard, kb_close_cb, LV_EVENT_CANCEL, NULL);
}
