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

/* El borde son 6 grados, que es lo que aguanta un frigo de absorcion de morro a
 * cola segun la especificacion de Dometic (3 de lado a lado). Empezo en 15, con
 * lo que todo lo util quedaba en el primer cuarto del circulo y la bola apenas
 * se movia. */
/* 120 de radio = 240 px de dial, el maximo que entra a lo ancho: 240 del dial
 * + 200 de la columna de lecturas + los 10 de margen a cada lado suman 460 de
 * los 480. De alto sobra (240 de 300). */
#define LEVEL_RADIUS    120  /* px, circulo exterior */
#define BUBBLE_RADIUS   14   /* px, burbuja */
#define MAX_DEG_SHOWN   6.0f  /* a partir de esto la burbuja se pega al borde */

/* Por debajo de esto se da por nivelada.
 *
 * Empezo en 0,5 y era irreal: medio centimetro por metro, mas fino que la
 * precision del propio montaje del sensor, y no lo pide ni el frigo ni la
 * espalda. Los numeros de verdad (Dometic, referencia del sector): un frigo de
 * absorcion aguanta 3 grados de lado a lado y 6 de morro a cola, y avisan de no
 * dejarlo desnivelado mas de 1-2 horas funcionando, porque el amoniaco
 * cristaliza y taponaria el circuito. Para dormir, la pendiente se empieza a
 * notar sobre 1-2 grados.
 *
 * Asi que 1 grado para el verde: comodo para dormir y de sobra para el frigo.
 * Decision del usuario (21-ago-2026). */
#define NIVELADO_DEG    1.0f

/* Anillos de referencia rotulados. Sin ellos la bola te dice hacia donde, pero
 * no CUANTO: habia que bajar la vista al texto para enterarse.
 *
 * Van en SEMAFORO, de dentro afuera, y la bola toma el color del anillo en el
 * que esta: asi el color solo ya dice si vas bien, sin comparar posiciones ni
 * leer los numeros. */
/* Lo que aguanta un frigo de absorcion NO es igual en los dos ejes (Dometic):
 * 3 grados de lado a lado y 6 de morro a cola, porque va montado de costado.
 * Asi que la zona aceptable es un OVALO, no un circulo: estrecho en balanceo y
 * alto en cabeceo. Dibujarla redonda seria mentir por los dos lados a la vez --
 * te asustaria de mas cabeceando y de menos balanceando. */
#define AMBAR_ROLL_DEG    3.0f   /* lado a lado */
#define AMBAR_PITCH_DEG   6.0f   /* morro a cola */

#define COL_NIVEL   0x4CD964   /* verde - nivelada, hasta 1 grado */
#define COL_CASI    0xFFD54F   /* ambar - hasta 3, el limite del frigo */
#define COL_MAL     0xFF4444   /* rojo  - mas de 3 */

/* Radio en pixeles de una inclinacion dada. */
#define RADIO_DE(grados) ((int)(((grados) / MAX_DEG_SHOWN) * LEVEL_RADIUS))

static lv_obj_t *s_circle;
static lv_obj_t *s_bubble;
static lv_obj_t *s_label_deg;
static lv_obj_t *s_label_status;
static lv_obj_t *s_ori_bm;          /* 0 / 90 / 180 / 270 */

static uint16_t indice_a_grados(uint16_t i) { return (uint16_t)(i * 90); }
static uint16_t grados_a_indice(uint16_t g) { return (uint16_t)(g / 90); }

static void ori_cb(lv_event_t *e)
{
    (void)e;
    uint16_t i = lv_btnmatrix_get_selected_btn(s_ori_bm);
    if (i > 3) return;
    uint16_t g = indice_a_grados(i);
    if (g == tilt_get_orientacion()) return;
    tilt_set_orientacion(g);
    /* Cambiar la orientacion borra la calibracion, y callarselo dejaria el
     * nivel torcido sin que nadie sepa por que. */
    lv_label_set_text(s_label_status, "Orientacion cambiada.\nVuelve a calibrar.");
}
static lv_obj_t *s_label_nivel;   /* "NIVELADA", aparte del estado */
static lv_timer_t *s_timer;

/* El color mira cada eje con SU vara, igual que el ovalo que se dibuja. */
static lv_color_t color_for_level(float pitch, float roll)
{
    float ap = fabsf(pitch), ar = fabsf(roll);
    if (ap <= NIVELADO_DEG && ar <= NIVELADO_DEG) return lv_color_hex(COL_NIVEL);
    if (ap <= AMBAR_PITCH_DEG && ar <= AMBAR_ROLL_DEG) return lv_color_hex(COL_CASI);
    return lv_color_hex(COL_MAL);
}

/* Zona aceptable, en forma de ovalo. LVGL no dibuja elipses, pero un rectangulo
 * con el radio al maximo da una "pastilla" -- semicirculos arriba y abajo con
 * los lados rectos -- que a estas proporciones se lee igual de bien. Se mete 4
 * px para dentro porque a 6 grados el alto coincide justo con el borde del dial
 * y se saldria por encima de su propio marco. */
static void make_ovalo(lv_obj_t *padre)
{
    lv_obj_t *o = lv_obj_create(padre);
    lv_obj_set_size(o, RADIO_DE(AMBAR_ROLL_DEG) * 2,
                       RADIO_DE(AMBAR_PITCH_DEG) * 2 - 4);
    lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_opa(o, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(o, 2, 0);
    lv_obj_set_style_border_color(o, lv_color_hex(COL_CASI), 0);
    lv_obj_set_style_pad_all(o, 0, 0);
    lv_obj_clear_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_center(o);
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
static void make_rotulo_anillo(lv_obj_t *padre, int radio, const char *txt,
                                uint32_t color)
{
    lv_obj_t *l = lv_label_create(padre);
    lv_label_set_text(l, txt);
    lv_obj_set_style_text_color(l, lv_color_hex(color), 0);
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

    lv_color_t col = color_for_level(pitch, roll);
    lv_obj_set_style_bg_color(s_bubble, col, 0);

    /* Que este nivelada se dice ADEMAS con palabras: el color solo no vale si
     * lo miras de reojo desde fuera del vehiculo, colocando las rampas. */
    /* En su propia linea y no en la de estado: esa la usa el boton de calibrar
     * ("Calibrando...", "Calibrado") y el refresco de 200 ms se la comeria. */
    lv_label_set_text(s_label_nivel,
                      (fabsf(pitch) <= NIVELADO_DEG && fabsf(roll) <= NIVELADO_DEG)
                      ? "NIVELADA" : "");

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
    /* Rojo FIJO: es el ultimo escalon del semaforo (mas de 5 grados). Antes
     * cambiaba de color con la inclinacion, pero ahora eso lo dice la bola, y
     * dos cosas cambiando a la vez confunden mas que informan. */
    lv_obj_set_style_border_color(s_circle, lv_color_hex(COL_MAL), 0);
    lv_obj_clear_flag(s_circle, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_center(s_circle);

    /* Anillos de referencia, de fuera adentro. El del centro es la zona de
     * NIVELADO y va en verde: cuando la bola entra ahi, ya puedes parar. Es
     * pequeno (0,5 grados a esta escala son 5 px) y la propia bola lo tapa --
     * a proposito: taparlo ES la senal. */
    make_ovalo(s_circle);
    make_anillo(s_circle, RADIO_DE(NIVELADO_DEG), COL_NIVEL, 2);

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
    make_rotulo_anillo(s_circle, RADIO_DE(NIVELADO_DEG), "1", COL_NIVEL);
    make_rotulo_anillo(s_circle, RADIO_DE(AMBAR_ROLL_DEG), "3", COL_CASI);
    make_rotulo_anillo(s_circle, LEVEL_RADIUS, "6\xC2\xB0", COL_MAL);

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
    lv_obj_set_size(right, 200, lv_pct(100));   /* 20 px cedidos al dial */
    lv_obj_set_style_bg_opa(right, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(right, 0, 0);
    lv_obj_set_style_pad_all(right, 0, 0);
    lv_obj_set_flex_flow(right, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(right, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    /* 8 y no 12: con el selector de orientacion son siete elementos apilados y
     * a 12 la columna sumaba 311 de los 320 disponibles -- cabia por nueve
     * pixeles, y el aviso de "vuelve a calibrar" ocupa dos lineas. */
    lv_obj_set_style_pad_row(right, 8, 0);
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

    /* Orientacion del sensor. Va AQUI y no en Ajustes a proposito: es el unico
     * sitio donde ves el efecto de tocarlo -- pones 90, inclinas el aparato y
     * compruebas si la bola va a donde debe. En Ajustes habria que ir y volver
     * a ciegas.
     *
     * Debajo del titulo y por encima del boton de calibrar, que es el orden en
     * que hay que usarlos: primero aciertas la orientacion, luego calibras. */
    lv_obj_t *ori_lbl = lv_label_create(right);
    lv_label_set_text(ori_lbl, "Sensor girado");
    lv_obj_set_style_text_color(ori_lbl, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(ori_lbl, &lv_font_montserrat_14, 0);

    static const char *ori_map[] = { "0", "90", "180", "270", "" };
    s_ori_bm = lv_btnmatrix_create(right);
    lv_btnmatrix_set_map(s_ori_bm, ori_map);
    lv_btnmatrix_set_one_checked(s_ori_bm, true);
    lv_obj_set_size(s_ori_bm, 170, 40);
    lv_obj_set_style_pad_all(s_ori_bm, 2, 0);
    lv_obj_set_style_border_width(s_ori_bm, 0, 0);
    lv_obj_set_style_bg_opa(s_ori_bm, LV_OPA_TRANSP, 0);
    lv_obj_set_style_text_font(s_ori_bm, &lv_font_montserrat_16, 0);
    for (uint16_t i = 0; i < 4; i++) {
        lv_btnmatrix_set_btn_ctrl(s_ori_bm, i, LV_BTNMATRIX_CTRL_CHECKABLE);
    }
    lv_btnmatrix_set_btn_ctrl(s_ori_bm, grados_a_indice(tilt_get_orientacion()),
                              LV_BTNMATRIX_CTRL_CHECKED);
    /* En RELEASED y no en VALUE_CHANGED: la marca del btnmatrix no se aplica
     * hasta soltar, y actuar al presionar deja el boton marcado en el sitio
     * viejo. Mismo motivo que en los registros. */
    lv_obj_add_event_cb(s_ori_bm, ori_cb, LV_EVENT_RELEASED, NULL);

    lv_obj_t *calib_btn = lv_btn_create(right);
    lv_obj_set_size(calib_btn, 170, 42);
    lv_obj_set_style_bg_color(calib_btn, lv_color_hex(0x333333), 0);
    lv_obj_add_event_cb(calib_btn, calib_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *calib_lbl = lv_label_create(calib_btn);
    lv_label_set_text(calib_lbl, "Calibrar nivel");
    lv_obj_center(calib_lbl);

    s_timer = lv_timer_create(refresh_cb, 200, NULL);
}
