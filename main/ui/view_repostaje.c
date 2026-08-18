/* view_repostaje.c - Formularios de repostaje y cambio de bombona (Fase 2).
 *
 * Dos pestanas (lv_tabview): "Repostaje" (km/precio/litros) y "Bombona"
 * (dia/hora/lugar), cada campo con teclado en pantalla compartido. El
 * boton "Guardar" de cada pestana solo loguea por ahora -- el envio real
 * a la P4 (mini_cmd_t nuevo, socket receptor nuevo en ~/joint/victron)
 * es la Fase 4, deliberadamente fuera de esta pasada.
 */
#include "view_repostaje.h"
#include "esp_log.h"

static const char *TAG = "view_repostaje";
static lv_obj_t *s_keyboard;

static void ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    bool numeric = (bool)(uintptr_t)lv_event_get_user_data(e);
    lv_keyboard_set_textarea(s_keyboard, ta);
    lv_keyboard_set_mode(s_keyboard, numeric ? LV_KEYBOARD_MODE_NUMBER : LV_KEYBOARD_MODE_TEXT_LOWER);
    lv_obj_clear_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
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
    lv_obj_set_size(cont, lv_pct(100), 56);
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
    lv_obj_set_size(ta, lv_pct(100), 34);
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 0, 18);
    if (numeric) {
        lv_textarea_set_accepted_chars(ta, "0123456789.");
    }
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_FOCUSED, (void *)(uintptr_t)numeric);
    lv_obj_add_event_cb(ta, ta_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    return ta;
}

static void save_repostaje_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Guardar repostaje pulsado -- TODO Fase 4: enviar por mini_cmd_t a la P4");
}

static void save_bombona_cb(lv_event_t *e)
{
    (void)e;
    ESP_LOGI(TAG, "Guardar cambio de bombona pulsado -- TODO Fase 4: enviar por mini_cmd_t a la P4");
}

static lv_obj_t *make_save_button(lv_obj_t *parent, const char *text, lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E7D32), 0);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_center(lbl);
    return btn;
}

void view_repostaje_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *tabview = lv_tabview_create(parent, LV_DIR_TOP, 36);
    lv_obj_set_size(tabview, lv_pct(100), lv_pct(100));

    lv_obj_t *tab_repo = lv_tabview_add_tab(tabview, "Repostaje");
    lv_obj_t *tab_gas  = lv_tabview_add_tab(tabview, "Bombona");
    lv_obj_set_flex_flow(tab_repo, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_flow(tab_gas, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(tab_repo, 6, 0);
    lv_obj_set_style_pad_row(tab_gas, 6, 0);

    make_field(tab_repo, "Kilometros", "0", true);
    make_field(tab_repo, "Precio (EUR/L)", "0.00", true);
    make_field(tab_repo, "Litros", "0.0", true);
    make_save_button(tab_repo, "Guardar repostaje", save_repostaje_cb);

    make_field(tab_gas, "Dia (DD/MM/AAAA)", "18/08/2026", false);
    make_field(tab_gas, "Hora (HH:MM)", "12:00", false);
    make_field(tab_gas, "Lugar", "", false);
    make_save_button(tab_gas, "Guardar cambio de bombona", save_bombona_cb);

    s_keyboard = lv_keyboard_create(parent);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_keyboard, kb_close_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_keyboard, kb_close_cb, LV_EVENT_CANCEL, NULL);
}
