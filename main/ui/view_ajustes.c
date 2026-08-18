/* view_ajustes.c - Ajustes Wi-Fi: SSID/password de la P4 a la que se
 * asocia, editables en el propio dispositivo (guardado en NVS via
 * net/udp_rx.c). Resuelve el mismo problema que ya sufrio victron_mini
 * (password fija en firmware, sin forma de cambiar de P4 sin reflashear).
 */
#include "view_ajustes.h"
#include "../net/udp_rx.h"
#include "nav.h"
#include "esp_log.h"
#include <string.h>

static const char *TAG = "view_ajustes";

static lv_obj_t *s_keyboard;
static lv_obj_t *s_ssid_ta;
static lv_obj_t *s_pass_ta;
static lv_obj_t *s_status_lbl;

static void ta_focus_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    lv_keyboard_set_textarea(s_keyboard, ta);
    lv_keyboard_set_mode(s_keyboard, LV_KEYBOARD_MODE_TEXT_LOWER);
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

static lv_obj_t *make_field(lv_obj_t *parent, const char *label_text)
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
    lv_obj_set_size(ta, lv_pct(100), 34);
    lv_obj_align(ta, LV_ALIGN_TOP_LEFT, 0, 18);
    lv_obj_add_event_cb(ta, ta_focus_cb, LV_EVENT_FOCUSED, NULL);
    lv_obj_add_event_cb(ta, ta_defocus_cb, LV_EVENT_DEFOCUSED, NULL);

    return ta;
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    nav_close_ajustes();
}

static void guardar_cb(lv_event_t *e)
{
    (void)e;
    const char *ssid = lv_textarea_get_text(s_ssid_ta);
    const char *pass = lv_textarea_get_text(s_pass_ta);
    udp_rx_set_credentials(ssid, pass);
    lv_label_set_text(s_status_lbl, "Guardado, reconectando...");
    ESP_LOGI(TAG, "Nuevas credenciales guardadas desde Ajustes");
}

void view_ajustes_refresh(void)
{
    char ssid[33], pass[65];
    udp_rx_get_credentials(ssid, sizeof(ssid), pass, sizeof(pass));
    lv_textarea_set_text(s_ssid_ta, ssid);
    lv_textarea_set_text(s_pass_ta, pass);
    lv_label_set_text(s_status_lbl, "");
}

void view_ajustes_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 4, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *row = lv_obj_create(parent);
    lv_obj_set_size(row, lv_pct(100), 34);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_btn_create(row);
    lv_obj_set_size(back, 80, 32);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x333333), 0);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Volver");
    lv_obj_center(back_lbl);

    lv_obj_t *title = lv_label_create(row);
    lv_label_set_text(title, "AJUSTES WI-FI");
    lv_obj_set_style_text_color(title, lv_color_hex(0xFF9800), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_RIGHT_MID, 0, 0);

    lv_obj_t *hint = lv_label_create(parent);
    lv_label_set_text(hint, "Red de la P4 a la que se asocia la 35cabina\n(cambiar aqui al pasar a otra P4, sin reflashear)");
    lv_obj_set_style_text_color(hint, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(hint, LV_TEXT_ALIGN_LEFT, 0);
    lv_obj_set_width(hint, lv_pct(100));

    s_ssid_ta = make_field(parent, "SSID");
    s_pass_ta = make_field(parent, "Password");

    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_style_bg_color(btn, lv_color_hex(0x2E7D32), 0);
    lv_obj_add_event_cb(btn, guardar_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Guardar y reconectar");
    lv_obj_center(btn_lbl);

    s_status_lbl = lv_label_create(parent);
    lv_label_set_text(s_status_lbl, "");
    lv_obj_set_style_text_color(s_status_lbl, lv_color_hex(0x4CD964), 0);
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_14, 0);

    s_keyboard = lv_keyboard_create(parent);
    lv_obj_add_flag(s_keyboard, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_keyboard, kb_close_cb, LV_EVENT_READY, NULL);
    lv_obj_add_event_cb(s_keyboard, kb_close_cb, LV_EVENT_CANCEL, NULL);
}
