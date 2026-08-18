/* view_inclinacion.c - Nivel de burbuja para aparcar (Fase 3).
 *
 * Burbuja clasica: un circulo pequeno que se desplaza dentro de un circulo
 * grande segun pitch/roll -- no hace falta ningun asset de imagen (no hay
 * pipeline de conversion PNG->LVGL en este entorno), son solo lv_obj
 * circulares con lv_obj_align + offset en pixeles. Color verde/ambar/rojo
 * segun cuanto se aleja de nivel, igual que el resto de la app.
 *
 * Layout en fila (circulo a la izda, info a la dcha): la resolucion
 * logica de la pantalla es LANDSCAPE 480x320 (ver ui_theme.h), apilar
 * todo verticalmente como en un primer intento no cabia comodo en solo
 * 320px de alto -- corregido 18-ago-2026.
 */
#include "view_inclinacion.h"
#include "../tilt.h"
#include <stdio.h>
#include <math.h>

#define LEVEL_RADIUS    70   /* px, circulo exterior */
#define BUBBLE_RADIUS   14   /* px, burbuja */
#define MAX_DEG_SHOWN   15.0f /* a partir de esto la burbuja se pega al borde */

static lv_obj_t *s_circle;
static lv_obj_t *s_bubble;
static lv_obj_t *s_label_deg;
static lv_obj_t *s_label_status;
static lv_timer_t *s_timer;

static lv_color_t color_for_level(float mag_deg)
{
    if (mag_deg <= 2.0f) return lv_color_hex(0x4CD964);
    if (mag_deg <= 6.0f) return lv_color_hex(0xFFD54F);
    return lv_color_hex(0xFF4444);
}

static void calib_btn_cb(lv_event_t *e)
{
    (void)e;
    lv_label_set_text(s_label_status, "Calibrando...");
    tilt_calibrate();   /* bloquea ~0.5s (ver tilt.h) */
    lv_label_set_text(s_label_status, "Calibrado");
}

static void refresh_cb(lv_timer_t *t)
{
    (void)t;
    if (!tilt_is_present()) {
        lv_obj_add_flag(s_bubble, LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_label_deg, "--");
        lv_label_set_text(s_label_status, "Sensor ADXL345 no detectado");
        return;
    }

    float pitch, roll;
    if (!tilt_get(&pitch, &roll)) {
        lv_label_set_text(s_label_status, "Error de lectura I2C");
        return;
    }
    lv_obj_clear_flag(s_bubble, LV_OBJ_FLAG_HIDDEN);

    float cr = roll  >  MAX_DEG_SHOWN ?  MAX_DEG_SHOWN : (roll  < -MAX_DEG_SHOWN ? -MAX_DEG_SHOWN : roll);
    float cp = pitch >  MAX_DEG_SHOWN ?  MAX_DEG_SHOWN : (pitch < -MAX_DEG_SHOWN ? -MAX_DEG_SHOWN : pitch);
    int off_x = (int)((cr / MAX_DEG_SHOWN) * (LEVEL_RADIUS - BUBBLE_RADIUS));
    int off_y = (int)((cp / MAX_DEG_SHOWN) * (LEVEL_RADIUS - BUBBLE_RADIUS));
    lv_obj_align(s_bubble, LV_ALIGN_CENTER, off_x, off_y);

    float mag = fabsf(pitch) > fabsf(roll) ? fabsf(pitch) : fabsf(roll);
    lv_color_t col = color_for_level(mag);
    lv_obj_set_style_bg_color(s_bubble, col, 0);
    lv_obj_set_style_border_color(s_circle, col, 0);

    char buf[48];
    snprintf(buf, sizeof(buf), "Cabeceo %+.1f\xC2\xB0\nBalanceo %+.1f\xC2\xB0", pitch, roll);
    lv_label_set_text(s_label_deg, buf);
    lv_label_set_text(s_label_status, "");
}

void view_inclinacion_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(parent, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(parent, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(parent, 10, 0);

    /* Columna izquierda: solo el circulo (le sobra alto de sobra en 320px) */
    lv_obj_t *left = lv_obj_create(parent);
    lv_obj_set_size(left, LEVEL_RADIUS * 2 + 8, LEVEL_RADIUS * 2 + 8);
    lv_obj_set_style_bg_opa(left, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(left, 0, 0);
    lv_obj_set_style_pad_all(left, 0, 0);
    lv_obj_clear_flag(left, LV_OBJ_FLAG_SCROLLABLE);

    s_circle = lv_obj_create(left);
    lv_obj_set_size(s_circle, LEVEL_RADIUS * 2, LEVEL_RADIUS * 2);
    lv_obj_set_style_radius(s_circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_circle, lv_color_hex(0x111111), 0);
    lv_obj_set_style_bg_opa(s_circle, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_circle, 3, 0);
    lv_obj_set_style_border_color(s_circle, lv_color_hex(0x4CD964), 0);
    lv_obj_clear_flag(s_circle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(s_circle);

    /* Cruz central de referencia (nivel = 0,0) */
    lv_obj_t *cross_h = lv_obj_create(s_circle);
    lv_obj_set_size(cross_h, LEVEL_RADIUS, 1);
    lv_obj_set_style_bg_color(cross_h, lv_color_hex(0x444444), 0);
    lv_obj_set_style_bg_opa(cross_h, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cross_h, 0, 0);
    lv_obj_clear_flag(cross_h, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(cross_h);

    lv_obj_t *cross_v = lv_obj_create(s_circle);
    lv_obj_set_size(cross_v, 1, LEVEL_RADIUS);
    lv_obj_set_style_bg_color(cross_v, lv_color_hex(0x444444), 0);
    lv_obj_set_style_bg_opa(cross_v, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cross_v, 0, 0);
    lv_obj_clear_flag(cross_v, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(cross_v);

    s_bubble = lv_obj_create(s_circle);
    lv_obj_set_size(s_bubble, BUBBLE_RADIUS * 2, BUBBLE_RADIUS * 2);
    lv_obj_set_style_radius(s_bubble, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(s_bubble, lv_color_hex(0x4CD964), 0);
    lv_obj_set_style_bg_opa(s_bubble, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_bubble, 0, 0);
    lv_obj_clear_flag(s_bubble, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(s_bubble);

    /* Columna derecha: titulo + lecturas + boton de calibrar, apilados */
    lv_obj_t *right = lv_obj_create(parent);
    lv_obj_set_size(right, 220, lv_pct(100));
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_pad_all(right, 0, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(right, 12, 0);
    lv_obj_clear_flag(right, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(right);
    lv_label_set_text(title, "INCLINACION");
    lv_obj_set_style_text_color(title, lv_color_hex(0xAB47BC), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);

    s_label_deg = lv_label_create(right);
    lv_label_set_text(s_label_deg, "--");
    lv_obj_set_style_text_color(s_label_deg, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(s_label_deg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_label_deg, LV_TEXT_ALIGN_CENTER, 0);

    s_label_status = lv_label_create(right);
    lv_label_set_text(s_label_status, "");
    lv_obj_set_style_text_color(s_label_status, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(s_label_status, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(s_label_status, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_label_status, lv_pct(100));

    lv_obj_t *calib_btn = lv_btn_create(right);
    lv_obj_set_size(calib_btn, 170, 42);
    lv_obj_set_style_bg_color(calib_btn, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(calib_btn, calib_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *calib_lbl = lv_label_create(calib_btn);
    lv_label_set_text(calib_lbl, "Calibrar nivel");
    lv_obj_center(calib_lbl);

    s_timer = lv_timer_create(refresh_cb, 200, NULL);
}
