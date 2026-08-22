/* view_ajustes.c - Ajustes Wi-Fi: SSID/password de la P4 a la que se
 * asocia, editables en el propio dispositivo (guardado en NVS via
 * net/udp_rx.c). Resuelve el mismo problema que ya sufrio victron_mini
 * (password fija en firmware, sin forma de cambiar de P4 sin reflashear).
 */
#include "view_ajustes.h"
#include "../net/udp_rx.h"
#include "confirm_screen.h"
#include "config_storage.h"
#include "entry_screen.h"
#include "nav.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "view_ajustes";

static lv_obj_t *s_ssid_ta;
static lv_obj_t *s_pass_ta;
static lv_obj_t *s_status_lbl;
/* Usuario y clave del PORTAL de la P4 -- NO son los del Wi-Fi. Hacen falta
 * desde que el portal exige Basic Auth tambien en el nivel abierto (21-ago-2026)
 * y la 3.5" le escribe apuntes de viaje. Se ven en la P4, en Ajustes -> Wi-Fi. */
static lv_obj_t *s_http_user_ta;
static lv_obj_t *s_http_pass_ta;

/* Lo que habia al abrir la pantalla, para saber si de verdad se ha cambiado
 * algo. Mismos tamanos que en udp_rx_get_credentials(). */
static char s_ssid_orig[33];
static char s_pass_orig[65];

/* Tocar un campo abre el editor a PANTALLA COMPLETA (entry_screen.c), el mismo
 * que usan los formularios de registros.
 *
 * Antes el teclado se creaba aqui dentro, y estaba mal: esta pantalla es una
 * columna flex, asi que el teclado entraba en el flujo como un elemento mas y
 * lo colocaban DEBAJO de todo lo demas -- se salia por el borde inferior y no
 * se veian las teclas de abajo. Con 320 px de alto no hay sitio para cabecera,
 * dos campos, boton y un teclado usable a la vez; el editor grande resuelve las
 * dos cosas, y de paso enseña el valor en letra 40 mientras lo tecleas. */
static void ta_click_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    entry_screen_open(ta, (const char *)lv_obj_get_user_data(ta), false);
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
    /* En CLICKED y no en FOCUSED: al volver del editor el campo conserva el
     * foco, asi que con FOCUSED el segundo toque no volveria a abrirlo. El
     * rotulo viaja en el user_data y es el titulo que saca el editor. */
    lv_obj_set_user_data(ta, (void *)label_text);
    lv_obj_add_event_cb(ta, ta_click_cb, LV_EVENT_CLICKED, NULL);

    return ta;
}

static void back_cb(lv_event_t *e)
{
    (void)e;
    nav_close_ajustes();
}

/* El cambio de verdad, ya confirmado. */
static void do_guardar(void *ud)
{
    (void)ud;
    const char *ssid = lv_textarea_get_text(s_ssid_ta);
    const char *pass = lv_textarea_get_text(s_pass_ta);
    udp_rx_set_credentials(ssid, pass);

    /* La referencia pasa a ser lo recien guardado: un segundo toque seguido ya
     * no es un cambio y no volvera a preguntar. */
    snprintf(s_ssid_orig, sizeof(s_ssid_orig), "%s", ssid);
    snprintf(s_pass_orig, sizeof(s_pass_orig), "%s", pass);

    save_portal_creds(lv_textarea_get_text(s_http_user_ta),
                      lv_textarea_get_text(s_http_pass_ta));

    lv_label_set_text(s_status_lbl, "Guardado, reconectando...");
    ESP_LOGI(TAG, "Nuevas credenciales guardadas desde Ajustes");
}

/* Cambiar la red no es un ajuste cualquiera: si te equivocas de SSID o de
 * contrasena la pantalla se queda sin la P4 y sin datos, y para volver atras
 * hay que teclear a mano lo de antes con el vehiculo en marcha. Por eso pide
 * confirmacion -- pero solo si de verdad has tocado algo, para no meter un
 * dialogo en el camino de quien solo estaba mirando. */
static void guardar_cb(lv_event_t *e)
{
    (void)e;
    const char *ssid = lv_textarea_get_text(s_ssid_ta);
    const char *pass = lv_textarea_get_text(s_pass_ta);

    bool cambio_red = strcmp(ssid, s_ssid_orig) != 0 || strcmp(pass, s_pass_orig) != 0;

    /* Las del portal se guardan SIN preguntar: equivocarse ahi no deja la
     * pantalla incomunicada, solo hace que la P4 rechace los apuntes con un
     * mensaje claro. La confirmacion se reserva para lo que si puede dejarte
     * sin datos, que es cambiar de red. */
    if (!cambio_red) {
        save_portal_creds(lv_textarea_get_text(s_http_user_ta),
                          lv_textarea_get_text(s_http_pass_ta));
        lv_label_set_text(s_status_lbl, "Guardado.");
        return;
    }

    static char body[64];   /* estatico: el dialogo lo sigue leyendo despues */
    snprintf(body, sizeof(body), "Red:  %s", ssid);
    /* Naranja, el mismo color del titulo de esta pantalla. */
    confirm_screen_open("Cambiar de red?", body, 0xFF9800,
                        "Si, cambiar", "Cancelar", do_guardar, NULL);
}

void view_ajustes_refresh(void)
{
    udp_rx_get_credentials(s_ssid_orig, sizeof(s_ssid_orig),
                           s_pass_orig, sizeof(s_pass_orig));
    lv_textarea_set_text(s_ssid_ta, s_ssid_orig);
    lv_textarea_set_text(s_pass_ta, s_pass_orig);

    char hu[33] = {0}, hp[65] = {0};
    size_t hul = sizeof(hu), hpl = sizeof(hp);
    load_portal_creds(hu, &hul, hp, &hpl);   /* si no hay nada, quedan vacios */
    lv_textarea_set_text(s_http_user_ta, hu);
    lv_textarea_set_text(s_http_pass_ta, hp);

    lv_label_set_text(s_status_lbl, "");
}

void view_ajustes_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 8, 0);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(parent, 4, 0);
    /* Cuatro campos y dos explicaciones NO caben en 320 px de alto, asi que
     * esta pantalla se desliza. Solo en vertical: en horizontal no hay nada que
     * ver y ademas confundiria con el gesto del carrusel (aunque Ajustes queda
     * fuera de el, el dedo no lo sabe). */
    lv_obj_set_scroll_dir(parent, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(parent, LV_SCROLLBAR_MODE_AUTO);

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

    lv_obj_t *hint2 = lv_label_create(parent);
    lv_label_set_text(hint2, "Usuario y clave del PORTAL de la P4 (no del wifi),\npara mandarle los apuntes del viaje");
    lv_obj_set_style_text_color(hint2, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(hint2, &lv_font_montserrat_14, 0);
    lv_obj_set_width(hint2, lv_pct(100));

    s_http_user_ta = make_field(parent, "Usuario del portal");
    s_http_pass_ta = make_field(parent, "Clave del portal");

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

}
