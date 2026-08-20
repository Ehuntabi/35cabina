/* confirm_screen.c - Confirmacion a pantalla completa. Ver confirm_screen.h. */
#include "confirm_screen.h"

/* Reparto de los 320 px de alto. Los botones van abajo del todo, que es donde
 * cae el pulgar, y el resumen se CENTRA en el hueco que queda entre el titulo y
 * ellos (de ahi el desplazamiento negativo): antes estaba pegado arriba y
 * dejaba mas de 120 px en negro.
 *
 * Cuerpo en letra 32, que es el siguiente tamano COMPILADO (el proyecto trae
 * 14/16/20/22/24/32/40; la 28 no existe y no compila). Para que quepa se
 * acortaron dos rotulos en build_resumen(): "Precio/litro:" -> "Precio/L:" y
 * "Precio total:" -> "Precio:". Con eso la linea mas larga ronda los 390 px de
 * los 451 utiles. Si se anaden campos, vigilar que ninguna pase de ~25
 * caracteres o partira en dos lineas. */
#define CONF_TITLE_Y   8
#define CONF_BODY_DY   -18
#define CONF_BTN_H     72
#define CONF_BTN_W     220

#define COL_FG         0x000000   /* texto sobre los botones claros */
#define COL_OK         0x66BB6A   /* verde  8,8:1 con el negro */
#define COL_NO         0xE57373   /* rojo   7,0:1 */

static lv_obj_t *s_root;
static lv_obj_t *s_title;
static lv_obj_t *s_body;
static lv_obj_t *s_ok_btn;
static lv_obj_t *s_ok_lbl;

static confirm_cb_t s_cb;
static void        *s_user_data;

static void close_overlay(void)
{
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    s_cb = NULL;
    s_user_data = NULL;
}

static void ok_cb(lv_event_t *e)
{
    (void)e;
    /* Se copian ANTES de cerrar: close_overlay() los borra, y la accion podria
     * abrir otro dialogo. */
    confirm_cb_t cb = s_cb;
    void *ud = s_user_data;
    close_overlay();
    if (cb) cb(ud);
}

static void no_cb(lv_event_t *e)
{
    (void)e;
    close_overlay();
}

static lv_obj_t *make_btn(lv_obj_t *parent, uint32_t color, lv_event_cb_t cb,
                           lv_align_t align, lv_obj_t **lbl_out)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, CONF_BTN_W, CONF_BTN_H);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(btn, lv_color_darken(lv_color_hex(color), LV_OPA_30),
                              LV_STATE_PRESSED);
    lv_obj_align(btn, align, align == LV_ALIGN_BOTTOM_LEFT ? 8 : -8, -8);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_FG), 0);
    lv_obj_center(lbl);
    if (lbl_out) *lbl_out = lbl;
    return btn;
}

void confirm_screen_init(lv_obj_t *parent)
{
    s_root = lv_obj_create(parent);
    lv_obj_set_size(s_root, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(s_root, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(s_root, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_root, 0, 0);
    lv_obj_set_style_pad_all(s_root, 0, 0);
    lv_obj_set_style_radius(s_root, 0, 0);
    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_add_flag(s_root, LV_OBJ_FLAG_HIDDEN);

    s_title = lv_label_create(s_root);
    lv_obj_set_width(s_title, lv_pct(94));
    lv_obj_set_style_text_font(s_title, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_align(s_title, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(s_title, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_title, LV_ALIGN_TOP_MID, 0, CONF_TITLE_Y);

    s_body = lv_label_create(s_root);
    lv_obj_set_width(s_body, lv_pct(96));
    lv_obj_set_style_text_font(s_body, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_body, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_align(s_body, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_line_space(s_body, 8, 0);
    lv_label_set_long_mode(s_body, LV_LABEL_LONG_WRAP);
    lv_obj_align(s_body, LV_ALIGN_CENTER, 0, CONF_BODY_DY);

    /* "No" a la izquierda y "Si" a la derecha: el destructivo lejos del pulgar
     * que viene de confirmar, y el orden habitual de lectura. */
    lv_obj_t *no_lbl;
    make_btn(s_root, COL_NO, no_cb, LV_ALIGN_BOTTOM_LEFT, &no_lbl);
    lv_label_set_text(no_lbl, "No, corregir");

    s_ok_btn = make_btn(s_root, COL_OK, ok_cb, LV_ALIGN_BOTTOM_RIGHT, &s_ok_lbl);
    lv_label_set_text(s_ok_lbl, "Si");
}

void confirm_screen_open(const char *title, const char *body,
                         uint32_t color, const char *ok_text,
                         confirm_cb_t cb, void *user_data)
{
    if (!s_root) return;

    s_cb = cb;
    s_user_data = user_data;

    lv_label_set_text(s_title, title ? title : "");
    lv_obj_set_style_text_color(s_title, lv_color_hex(color), 0);
    lv_label_set_text(s_body, (body && body[0]) ? body : "");
    lv_label_set_text(s_ok_lbl, ok_text ? ok_text : "Si");

    lv_obj_clear_flag(s_root, LV_OBJ_FLAG_HIDDEN);
    lv_obj_move_foreground(s_root);
}

void confirm_screen_close(void)
{
    if (s_root) close_overlay();
}
