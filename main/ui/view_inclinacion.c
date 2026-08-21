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

/* El borde son 10 grados y no 15: nivelando una autocaravana todo lo que
 * importa pasa entre 0 y 5, y con 15 ese recorrido se quedaba en el primer
 * cuarto del circulo, donde no se aprecia. */
#define LEVEL_RADIUS    100  /* px, circulo exterior */
#define BUBBLE_RADIUS   12   /* px, burbuja */
#define MAX_DEG_SHOWN   10.0f /* a partir de esto la burbuja se pega al borde */

/* Por debajo de esto se da por nivelada: es lo que se suele dar por bueno para
 * dormir sin notar la pendiente y para que el frigorifico de absorcion trabaje
 * bien. Decision del usuario (21-ago-2026). */
#define NIVELADO_DEG    0.5f

/* Anillos de referencia rotulados. Sin ellos la bola te dice hacia donde, pero
 * no CUANTO: habia que bajar la vista al texto para enterarse. */
#define ANILLO_1_DEG    2.0f
#define ANILLO_2_DEG    5.0f

/* Radio en pixeles de una inclinacion dada. */
#define RADIO_DE(grados) ((int)(((grados) / MAX_DEG_SHOWN) * LEVEL_RADIUS))

static lv_obj_t *s_circle;
static lv_obj_t *s_bubble;
static lv_obj_t *s_label_deg;
static lv_obj_t *s_label_status;
static lv_obj_t *s_label_nivel;   /* "NIVELADA", aparte del estado */
static lv_timer_t *s_timer;

static lv_color_t color_for_level(float mag_deg)
{
    if (mag_deg <= NIVELADO_DEG) return lv_color_hex(0x4CD964);   /* nivelada */
    if (mag_deg <= ANILLO_1_DEG) return lv_color_hex(0xFFD54F);   /* casi */
    return lv_color_hex(0xFF4444);
}

/* Un anillo de referencia: circulo hueco con el borde fino. Se crean ANTES que
 * la burbuja para que esta quede por encima. */
static void make_anillo(lv_obj_t *padre, int radio, uint32_t color, int grosor)
{
    lv_obj_t *a = lv_obj_create(padre);
    lv_obj_set_size(a, radio * 2, radio * 2);
    lv_obj_set_style_radius(a, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(a, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(a, grosor, 0);
    lv_obj_set_style_border_color(a, lv_color_hex(color), 0);
    lv_obj_set_style_pad_all(a, 0, 0);
    lv_obj_clear_flag(a, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(a);
}

/* Rotulo del anillo, sobre el eje horizontal y justo debajo de la linea: ahi no
 * pisa ni la cruz ni el recorrido vertical de la bola. */
static void make_rotulo_anillo(lv_obj_t *padre, int radio, const char *txt)
{
    lv_obj_t *l = lv_label_create(padre);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(0x777777), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_align(l, LV_ALIGN_CENTER, radio - 12, 10);
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
        lv_label_set_text(s_label_nivel, "");
        lv_label_set_text(s_label_status, "Sensor ADXL345 no detectado");
        return;
    }

    float pitch, roll;
    if (!tilt_get(&pitch, &roll)) {
        lv_label_set_text(s_label_nivel, "");
        lv_label_set_text(s_label_status, "Error de lectura I2C");
        return;
    }
    lv_obj_clear_flag(s_bubble, LV_OBJ_FLAG_HIDDEN);

    float cr = roll  >  MAX_DEG_SHOWN ?  MAX_DEG_SHOWN : (roll  < -MAX_DEG_SHOWN ? -MAX_DEG_SHOWN : roll);
    float cp = pitch >  MAX_DEG_SHOWN ?  MAX_DEG_SHOWN : (pitch < -MAX_DEG_SHOWN ? -MAX_DEG_SHOWN : pitch);
    /* La bola rueda al lado BAJO, como una canica: donde este la bola, ahi va la
     * rampa. Asi no hay que traducir nada mentalmente al nivelar.
     *
     * El balanceo ya salia asi, pero el cabeceo iba al reves (levantabas el
     * morro y la bola se iba hacia delante, o sea al lado ALTO): las dos
     * formulas de tilt.c no llevan el mismo signo, una tiene el menos y la otra
     * no. Se corrige AQUI, en el dibujo, y no en tilt.c a proposito: alli
     * cambiaria tambien el signo del angulo que se escribe debajo ("Cabeceo
     * +2.3") y el de la calibracion ya guardada en NVS, que no tienen nada de
     * malo. */
    int off_x = (int)((cr / MAX_DEG_SHOWN) * (LEVEL_RADIUS - BUBBLE_RADIUS));
    int off_y = -(int)((cp / MAX_DEG_SHOWN) * (LEVEL_RADIUS - BUBBLE_RADIUS));
    lv_obj_align(s_bubble, LV_ALIGN_CENTER, off_x, off_y);

    float mag = fabsf(pitch) > fabsf(roll) ? fabsf(pitch) : fabsf(roll);
    lv_color_t col = color_for_level(mag);
    lv_obj_set_style_bg_color(s_bubble, col, 0);
    lv_obj_set_style_border_color(s_circle, col, 0);

    /* Que este nivelada se dice ADEMAS con palabras: el color solo no vale si
     * lo miras de reojo desde fuera del vehiculo, colocando las rampas. */
    /* En su propia linea y no en la de estado: esa la usa el boton de calibrar
     * ("Calibrando...", "Calibrado") y el refresco de 200 ms se la comeria. */
    lv_label_set_text(s_label_nivel, mag <= NIVELADO_DEG ? "NIVELADA" : "");

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

    /* Anillos de referencia, de fuera adentro. El del centro es la zona de
     * NIVELADO y va en verde: cuando la bola entra ahi, ya puedes parar. Es
     * pequeno (0,5 grados a esta escala son 5 px) y la propia bola lo tapa --
     * a proposito: taparlo ES la senal. */
    make_anillo(s_circle, RADIO_DE(ANILLO_2_DEG), 0x555555, 1);
    make_anillo(s_circle, RADIO_DE(ANILLO_1_DEG), 0x555555, 1);
    make_anillo(s_circle, RADIO_DE(NIVELADO_DEG), 0x4CD964, 2);

    /* Cruz central de referencia (nivel = 0,0), de lado a lado */
    /* 2 px y gris claro: a 1 px y en 0x444444 sobre el fondo casi negro del
     * dial no se veia, que es como no tenerla. */
    lv_obj_t *cross_h = lv_obj_create(s_circle);
    lv_obj_set_size(cross_h, LEVEL_RADIUS * 2, 2);
    lv_obj_set_style_bg_color(cross_h, lv_color_hex(0x888888), 0);
    lv_obj_set_style_bg_opa(cross_h, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cross_h, 0, 0);
    lv_obj_clear_flag(cross_h, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(cross_h);

    lv_obj_t *cross_v = lv_obj_create(s_circle);
    lv_obj_set_size(cross_v, 2, LEVEL_RADIUS * 2);
    lv_obj_set_style_bg_color(cross_v, lv_color_hex(0x888888), 0);
    lv_obj_set_style_bg_opa(cross_v, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(cross_v, 0, 0);
    lv_obj_clear_flag(cross_v, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(cross_v);

    /* Cuanto vale cada anillo, escrito. Sin esto los anillos decoran pero no
     * miden. */
    make_rotulo_anillo(s_circle, RADIO_DE(ANILLO_1_DEG), "2");
    make_rotulo_anillo(s_circle, RADIO_DE(ANILLO_2_DEG), "5");
    make_rotulo_anillo(s_circle, LEVEL_RADIUS, "10\xC2\xB0");

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
    lv_obj_set_style_text_font(s_label_deg, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_align(s_label_deg, LV_TEXT_ALIGN_CENTER, 0);

    s_label_nivel = lv_label_create(right);
    lv_label_set_text(s_label_nivel, "");
    lv_obj_set_style_text_color(s_label_nivel, lv_color_hex(0x4CD964), 0);
    lv_obj_set_style_text_font(s_label_nivel, &lv_font_montserrat_22, 0);

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
