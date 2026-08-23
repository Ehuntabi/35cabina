/* view_info.c - Pantalla de info agrupada (Fase 1).
 *
 * Patron de colores/umbrales y LEDs de agua portado de
 * ~/joint/victron_mini/main/ui/view_quad.c, reescrito como grid (todas las
 * cards visibles a la vez, sin rotacion) porque aqui hay mucho mas sitio
 * que en la pantalla de 320x172 del mini.
 *
 * REPARTO (rehecho el 22-ago-2026: la version anterior eran CINCO tarjetas del
 * mismo peso y el usuario la describio como "horrible" -- todo pesaba igual,
 * los numeros eran pequenos y habia demasiados bordes de colores):
 *
 *   +--------------------------------------------------+
 *   |  BATERIA          62%   13,43 V  -3,0 A   MOTOR  |  <- toda la franja
 *   |  ##################............          12,8 V  |     de arriba
 *   +---------------------------+----------------------+
 *   |  AGUAS                    |  TEMPERATURAS        |
 *   +---------------------------+----------------------+
 *
 * La bateria manda: es lo que se mira el 90% de las veces, asi que se lleva la
 * franja entera, el numero mas grande de la pantalla y una BARRA de carga que
 * antes no existia. La del motor va dentro, que es bateria tambien y solo tiene
 * un dato. Las dos temperaturas comparten tarjeta (peticion del usuario) y
 * aguas encoge: antes ocupaba un cuarto de la pantalla para mostrar cinco
 * lucecitas.
 */
#include "view_info.h"
#include "nav.h"
#include "../data_model.h"
#include "esp_timer.h"
#include <stdio.h>

#define COL_CARD_BG_TOP  lv_color_hex(0x0A0A0A)
#define COL_CARD_BG_BOT  lv_color_hex(0x161616)
#define COL_TEXT         lv_color_hex(0xFFFFFF)
#define COL_TEXT_DIM     lv_color_hex(0x888888)
/* Mas claro que COL_TEXT_DIM: para rotulos que hay que poder leer de un
 * vistazo (la escala de aguas, los nombres de las temperaturas) y no solo
 * intuir. El 0x888888 vale para lo accesorio, no para esto. */
#define COL_TEXT_ESCALA  lv_color_hex(0xCCCCCC)
#define COL_VAL_GOOD     lv_color_hex(0x4CD964)
#define COL_VAL_WARN     lv_color_hex(0xFFD54F)
#define COL_VAL_BAD      lv_color_hex(0xFF4444)
#define COL_CONN_NONE    lv_color_hex(0x555555)
#define COL_CONN_OK      lv_color_hex(0x00C851)
#define COL_CONN_LOST    lv_color_hex(0xFF4444)
#define CONN_TIMEOUT_MS  5000

#define COL_BORDER_BAT   lv_color_hex(0xFF9800)  /* SmartShunt naranja */
#define COL_BORDER_AUX   lv_color_hex(0x4FC3F7)  /* Bateria motor cyan */
#define COL_BORDER_COLD  lv_color_hex(0x66CCFF)  /* Frigo azul claro */
#define COL_BORDER_WATER lv_color_hex(0x29B6F6)  /* Aguas azul saturado */
#define COL_BORDER_HEAT  lv_color_hex(0xFFD54F)  /* Exterior amarillo calido */

/* Bateria: la tarjeta grande de arriba, con sus piezas sueltas. */
static lv_obj_t   *s_bat_card;
static lv_obj_t   *s_bat_dot;
static lv_obj_t   *s_bat_soc;      /* el numero, DENTRO del dibujo */
static lv_obj_t   *s_bat_volt;     /* solo el numero, alineado a la derecha */
static lv_obj_t   *s_bat_amp;
static lv_obj_t   *s_bat_amp_u;    /* la "A", que se tine con el signo */
static lv_obj_t   *s_aux_val;      /* bateria motor, en la misma tarjeta */

/* Abajo: aguas a la izquierda, las dos temperaturas a la derecha. */
static lv_obj_t   *s_water_card;
static lv_obj_t   *s_led_clean[4];
static lv_obj_t   *s_led_gray;
static lv_obj_t   *s_lbl_gray;     /* el rotulo, que tambien avisa */
static lv_obj_t   *s_frigo_val;
static lv_obj_t   *s_frigo_trend;  /* flecha de tendencia */
static lv_obj_t   *s_ext_trend;
static lv_obj_t   *s_pendientes;      /* pastilla "N sin enviar", oculta si 0 */

/* Definidas abajo, con el resto de la pastilla. */
static void pendientes_aplicar(void *arg);
static void pendientes_click_cb(lv_event_t *e);
static lv_obj_t   *s_gps;             /* indicador de GPS de la P4 */
static lv_obj_t   *s_frigo_fan;
static lv_obj_t   *s_ext_val;
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

/* === Dibujo de bateria de coche ==========================================
 *
 * El porcentaje NO es un numero al lado de una barra: es el RELLENO de una
 * bateria dibujada, que sube y baja y cambia de color. Se entiende sin leer.
 *
 * Todo con lv_obj rectangulares, sin imagenes: en este entorno no hay pipeline
 * de PNG a LVGL, y ademas asi escala y cambia de color solo. Mismo criterio que
 * la burbuja del nivel.
 *
 *      ##   ##      <- bornes
 *    +--------+
 *    |        |
 *    |%%%%%%%%|     <- relleno, pegado abajo, alto proporcional al SoC
 *    +--------+
 */
#define BAT_W        118
#define BAT_H         86
#define BAT_BORNE_W   22
#define BAT_BORNE_H    8

/* Hueco fijo del numero de tension/corriente, con la unidad justo detras. Ver
 * el porque en view_info_create. */
#define BAT_NUM_W     104
#define BAT_NUM_X      12

/* Hueco fijo de las temperaturas, con la flecha de tendencia detras. */
#define TEMP_NUM_W    120

static lv_obj_t *s_bat_relleno;

static lv_obj_t *make_bateria_dibujo(lv_obj_t *padre)
{
    /* Contenedor del conjunto: cuerpo + los dos bornes que sobresalen arriba. */
    lv_obj_t *cont = lv_obj_create(padre);
    lv_obj_set_size(cont, BAT_W, BAT_H + BAT_BORNE_H);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    for (int i = 0; i < 2; i++) {
        lv_obj_t *borne = lv_obj_create(cont);
        lv_obj_set_size(borne, BAT_BORNE_W, BAT_BORNE_H + 4);   /* +4: se meten
                                                                 * bajo el cuerpo
                                                                 * para que no se
                                                                 * vea la union */
        lv_obj_set_style_bg_color(borne, lv_color_hex(0x888888), 0);
        lv_obj_set_style_bg_opa(borne, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(borne, 0, 0);
        lv_obj_set_style_radius(borne, 2, 0);
        lv_obj_set_style_pad_all(borne, 0, 0);
        lv_obj_clear_flag(borne, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(borne, LV_ALIGN_TOP_LEFT, i == 0 ? 14 : BAT_W - 14 - BAT_BORNE_W, 0);
    }

    lv_obj_t *cuerpo = lv_obj_create(cont);
    lv_obj_set_size(cuerpo, BAT_W, BAT_H);
    lv_obj_set_style_bg_color(cuerpo, lv_color_hex(0x0E0E0E), 0);
    lv_obj_set_style_bg_opa(cuerpo, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(cuerpo, lv_color_hex(0x888888), 0);
    lv_obj_set_style_border_width(cuerpo, 3, 0);
    lv_obj_set_style_radius(cuerpo, 6, 0);
    lv_obj_set_style_pad_all(cuerpo, 0, 0);
    lv_obj_clear_flag(cuerpo, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(cuerpo, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* El relleno crece desde ABAJO: se ancla al fondo y solo cambia de alto. */
    s_bat_relleno = lv_obj_create(cuerpo);
    lv_obj_set_width(s_bat_relleno, lv_pct(100));
    lv_obj_set_height(s_bat_relleno, 0);
    lv_obj_set_style_bg_opa(s_bat_relleno, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(s_bat_relleno, 0, 0);
    lv_obj_set_style_radius(s_bat_relleno, 3, 0);
    lv_obj_set_style_pad_all(s_bat_relleno, 0, 0);
    lv_obj_clear_flag(s_bat_relleno, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    lv_obj_align(s_bat_relleno, LV_ALIGN_BOTTOM_MID, 0, 0);

    /* El numero va DENTRO y encima del relleno (se crea despues, asi queda por
     * delante en el orden de dibujo). */
    s_bat_soc = lv_label_create(cuerpo);
    lv_label_set_text(s_bat_soc, "--");
    lv_obj_set_style_text_font(s_bat_soc, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_color(s_bat_soc, COL_TEXT, 0);
    lv_obj_center(s_bat_soc);

    return cont;
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

/* Medidas del indicador de aguas, calculadas para el hueco que hay:
 *
 * La tarjeta mide ~123 px de alto; menos el relleno y el borde quedan ~103, y
 * el titulo se lleva ~25. O sea unos 78 px para los indicadores, rotulo suyo
 * incluido.
 *
 * Con 13 de segmento salian 77 de 78 y SE RECORTABA: el bloque de grises
 * aparecia cortado por la mitad, que es como se noto. Ahora: cuatro segmentos
 * de 11 y tres huecos de 3 son 53, mas 3 y el rotulo de 17 = 73. Con margen.
 *
 * Los de limpia son SEGMENTOS anchos y bajos, no cuadraditos: apilados leen
 * como un deposito. El de grises es un circulo del alto de dos segmentos, para
 * que pese lo mismo que la columna sin ser un puntito. */
#define LED_SEG_W    56
/* 17 de alto y no 11: cada segmento lleva a su izquierda su fraccion (1/4,
 * 2/4...) en letra 14, que ocupa 18 px de alto. La fila DEBE medir eso o mas:
 * con 17 el texto no cabia y LVGL lo recortaba entero -- no se veia ninguna
 * fraccion. Cuatro de 18 con huecos de 1 son 75, y con el titulo (25) suman 100
 * de los ~103 utiles de la tarjeta. */
#define LED_SEG_H    18
#define LED_SEG_GAP   1
#define LED_FRAC_W   32   /* hueco fijo de la fraccion, para que no se recorte */
/* Grises: MISMA forma y ancho que los segmentos de limpia, pero de una pieza en
 * vez de cuatro. Antes era un circulo, y un redondel grande al lado de una
 * columna de rectangulos quedaba raro. Se distingue de sobra por ser un bloque
 * unico y por su rotulo; no hace falta cambiarle la forma.
 * De ancho sobra: 56 + 18 de hueco + 56 son 130 de los ~214 utiles. */
/* Tan alto como la columna de limpia ENTERA, rotulo incluido: 53 de los cuatro
 * segmentos, un pelin menos. Su texto va DENTRO del bloque y no debajo: asi no
 * le roba alto ni obliga a encoger nada. */
#define LED_GRIS_H   (LED_SEG_H * 4 + LED_SEG_GAP * 3 - 10)

static lv_obj_t *make_led(lv_obj_t *parent, bool round)
{
    lv_obj_t *led = lv_obj_create(parent);
    lv_obj_set_size(led, LED_SEG_W, round ? LED_GRIS_H : LED_SEG_H);
    lv_obj_set_style_radius(led, 3, 0);
    lv_obj_set_style_border_width(led, 2, 0);
    lv_obj_set_style_pad_all(led, 0, 0);
    lv_obj_clear_flag(led, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    led_set(led, 0);
    return led;
}

static void make_water_cell(lv_obj_t *grid, uint8_t col, uint8_t span, uint8_t row)
{
    lv_obj_t *card = lv_obj_create(grid);
    lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, span, LV_GRID_ALIGN_STRETCH, row, 1);
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

    /* Esta tarjeta ya NO lleva punto de conexion. Habia dos, aqui y en la de
     * bateria, y los DOS se pintaban del mismo color calculado una sola vez:
     * eran el mismo indicador repetido. El usuario pregunto si significaban
     * cosas distintas -- que es justo lo que sugiere ver dos -- y no. Se queda
     * el de la tarjeta de bateria, que es la principal. */
    lv_obj_t *t = lv_label_create(card);
    lv_label_set_text(t, "AGUAS");
    lv_obj_set_style_text_color(t, COL_BORDER_WATER, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_20, 0);
    lv_obj_align(t, LV_ALIGN_TOP_MID, 0, 0);

    /* Los cuatro de agua limpia van en COLUMNA y se encienden de ABAJO ARRIBA,
     * como se llena un deposito de verdad. En fila no significaban nada: cuatro
     * lucecitas en horizontal no dicen "nivel".
     *
     * COLUMN_REVERSE es lo que pone el indice 0 abajo, que es el primero que se
     * enciende (ver refresh_aguas). Con COLUMN normal el deposito se llenaria
     * por el techo. */
    lv_obj_t *fila = lv_obj_create(card);
    lv_obj_set_size(fila, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(fila, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(fila, 0, 0);
    lv_obj_set_style_pad_all(fila, 0, 0);
    lv_obj_set_style_pad_column(fila, 18, 0);
    lv_obj_set_flex_flow(fila, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(fila, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(fila, LV_OBJ_FLAG_SCROLLABLE);
    /* Desplazada 10 px a la izquierda del centro: centrada del todo, la columna
     * de agua limpia con sus fracciones quedaba metida hacia dentro de la
     * tarjeta. Peticion del usuario, 23-ago-2026. */
    lv_obj_align(fila, LV_ALIGN_CENTER, -10, 8);

    /* Columna de segmentos de agua limpia, cada uno con su fraccion al lado */
    lv_obj_t *col_limpia = lv_obj_create(fila);
    lv_obj_set_size(col_limpia, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col_limpia, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col_limpia, 0, 0);
    lv_obj_set_style_pad_all(col_limpia, 0, 0);
    lv_obj_set_style_pad_row(col_limpia, 3, 0);
    lv_obj_set_flex_flow(col_limpia, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_limpia, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(col_limpia, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *leds = lv_obj_create(col_limpia);
    lv_obj_set_size(leds, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(leds, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(leds, 0, 0);
    lv_obj_set_style_pad_all(leds, 0, 0);
    lv_obj_set_style_pad_row(leds, LED_SEG_GAP, 0);
    lv_obj_set_flex_flow(leds, LV_FLEX_FLOW_COLUMN_REVERSE);
    lv_obj_set_flex_align(leds, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(leds, LV_OBJ_FLAG_SCROLLABLE);
    /* Cada segmento va en su propia fila junto a su fraccion, para que numero y
     * barra queden a la misma altura. El indice 0 es 1/4 y queda ABAJO
     * (COLUMN_REVERSE): el deposito se llena de abajo arriba. */
    static const char *const FRACCION[4] = { "1/4", "2/4", "3/4", "4/4" };
    for (int i = 0; i < 4; i++) {
        lv_obj_t *fila_seg = lv_obj_create(leds);
        lv_obj_set_size(fila_seg, LED_FRAC_W + 12 + LED_SEG_W, LED_SEG_H);
        lv_obj_set_style_bg_opa(fila_seg, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(fila_seg, 0, 0);
        lv_obj_set_style_pad_all(fila_seg, 0, 0);
        lv_obj_set_style_pad_column(fila_seg, 12, 0);
        lv_obj_set_flex_flow(fila_seg, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(fila_seg, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                              LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(fila_seg, LV_OBJ_FLAG_SCROLLABLE);

        lv_obj_t *frac = lv_label_create(fila_seg);
        lv_label_set_text(frac, FRACCION[i]);
        lv_obj_set_style_text_color(frac, COL_TEXT_ESCALA, 0);
        lv_obj_set_style_text_font(frac, &lv_font_montserrat_14, 0);
        lv_obj_set_width(frac, LED_FRAC_W);
        lv_obj_set_style_text_align(frac, LV_TEXT_ALIGN_RIGHT, 0);

        s_led_clean[i] = make_led(fila_seg, false);
    }


    /* Grises: uno solo, es alarma de lleno, no nivel */
    lv_obj_t *col_gris = lv_obj_create(fila);
    lv_obj_set_size(col_gris, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col_gris, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col_gris, 0, 0);
    lv_obj_set_style_pad_all(col_gris, 0, 0);
    lv_obj_set_style_pad_row(col_gris, 3, 0);
    lv_obj_set_flex_flow(col_gris, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_gris, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(col_gris, LV_OBJ_FLAG_SCROLLABLE);

    /* Mas grande que los de nivel: esto no es un nivel, es una ALARMA. Con el
     * deposito de grises lleno hay que vaciar antes de seguir usando el
     * fregadero, y en la P4 se ve como un tanque entero pintado de rojo --
     * aqui era un puntito de 16 px que pasaba desapercibido. */
    s_led_gray = make_led(col_gris, true);

    /* El rotulo va DENTRO del bloque para no robarle alto (ver LED_GRIS_H). */
    s_lbl_gray = lv_label_create(s_led_gray);
    lv_label_set_text(s_lbl_gray, "grises");
    lv_obj_set_style_text_color(s_lbl_gray, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_lbl_gray, &lv_font_montserrat_14, 0);
    lv_obj_center(s_lbl_gray);
}

/* === Refresco =========================================================== */

static void refresh_bat(const mini_data_t *d)
{
    char buf[32];
    if (d->has_data) {
        int soc = d->shunt_soc_deci / 10;
        if (soc < 0) soc = 0;
        if (soc > 100) soc = 100;
        /* El relleno usa los MISMOS umbrales de color que ya se usaban para el
         * numero: verde >=60, ambar >=30, rojo por debajo. */
        lv_color_t col = color_for_soc(d->shunt_soc_deci);

        snprintf(buf, sizeof(buf), "%d%%", soc);
        lv_label_set_text(s_bat_soc, buf);

        lv_obj_set_height(s_bat_relleno, (BAT_H - 6) * soc / 100);
        lv_obj_set_style_bg_color(s_bat_relleno, col, 0);
        /* Con la bateria llena el numero queda SOBRE el relleno, y el verde y
         * el ambar son claros: en blanco no se leeria. Se pasa a negro. */
        lv_obj_set_style_text_color(s_bat_soc,
                                    soc >= 45 ? lv_color_hex(0x000000) : COL_TEXT, 0);

        snprintf(buf, sizeof(buf), "%d.%02d",
                 d->shunt_voltage_centi / 100, d->shunt_voltage_centi % 100);
        lv_label_set_text(s_bat_volt, buf);

        int32_t ma = d->shunt_current_milli;
        char sgn = ma < 0 ? '-' : '+';
        int32_t am = ma < 0 ? -ma : ma;
        snprintf(buf, sizeof(buf), "%c%ld.%ld", sgn,
                 (long)(am / 1000), (long)((am % 1000) / 100));
        lv_label_set_text(s_bat_amp, buf);
        /* Verde cargando, NARANJA consumiendo: se ve si la bateria sube o baja
         * sin pararse a leer el signo. Naranja y no rojo a proposito -- gastar
         * es lo normal, no una averia; el rojo se reserva para el nivel bajo. */
        lv_color_t col_amp = ma >= 0 ? COL_VAL_GOOD : lv_color_hex(0xFF9800);
        lv_obj_set_style_text_color(s_bat_amp, col_amp, 0);
        lv_obj_set_style_text_color(s_bat_amp_u, col_amp, 0);   /* la A, a juego */
    } else {
        lv_label_set_text(s_bat_soc, "--");
        lv_obj_set_style_text_color(s_bat_soc, COL_TEXT, 0);
        lv_obj_set_height(s_bat_relleno, 0);
        /* "--" en los dos, igual que el SoC de arriba y que MOTOR. Antes ponia
         * "sin datos" en el hueco del voltaje y dejaba los amperios EN BLANCO:
         * el texto se salia de su caja de ancho fijo, y el hueco vacio de al
         * lado parecia un fallo de pintado en vez de una falta de dato. */
        lv_label_set_text(s_bat_volt, "--");
        lv_label_set_text(s_bat_amp, "--");
        /* Devolver el color neutro: si no, el "--" se queda del verde o el
         * naranja de la ultima lectura buena, como si siguiera cargando. */
        lv_obj_set_style_text_color(s_bat_amp, COL_TEXT, 0);
        lv_obj_set_style_text_color(s_bat_amp_u, COL_TEXT, 0);
    }
}

/* Bateria del motor: va DENTRO de la tarjeta de bateria, que tambien es
 * bateria y solo tiene un dato que ensenar.
 * Mismo criterio de formato que ui_format_aux_value() en la P4: aux_value_raw
 * crudo, la unidad depende de aux_input (0/1=V*100, 2=Kelvin*100). */
static void refresh_aux(const mini_data_t *d)
{
    char buf[32];
    if (d->aux_has_data) {
        /* El canal auxiliar del shunt esta configurado como bateria de arranque
         * (aux_input 0 = voltage2), que es lo que dice el titulo MOTOR. Los
         * otros dos modos que admite -- punto medio de un banco (1) y sonda de
         * temperatura (2) -- no se usan en esta instalacion, asi que no se
         * rotulan; si algun dia se configuraran, aqui saldria un voltaje o unos
         * grados sin avisar de cual es. */
        if (d->aux_input == 2) {
            int temp_centi = (int)d->aux_value_raw - 27315;
            int ti = temp_centi / 100;
            int td = (temp_centi >= 0 ? temp_centi : -temp_centi) % 100 / 10;
            snprintf(buf, sizeof(buf), "%d.%d\xC2\xB0" "C", ti, td);
        } else {
            snprintf(buf, sizeof(buf), "%d.%02d V",
                     d->aux_value_raw / 100, d->aux_value_raw % 100);
        }
        lv_label_set_text(s_aux_val, buf);
    } else {
        lv_label_set_text(s_aux_val, "--");
    }
}

/* Tendencia del frigo: flecha ARRIBA naranja si la temperatura esta subiendo,
 * ABAJO azul si baja o se mantiene.
 *
 * No se compara con la lectura anterior: el dato llega cada segundo y oscila
 * unas decimas, asi que la flecha estaria todo el rato cambiando y no diria
 * nada. Se compara con una media lenta (el 2% de cada muestra, o sea unos 50 s
 * de memoria) y ademas hay HISTERESIS: para pasar a "sube" hace falta estar
 * 0,3 grados por encima de la media, y para volver a "baja" hay que caer por
 * debajo de 0,05. Entre medias se queda como estaba, que es lo que evita el
 * parpadeo justo en el umbral.
 *
 * "Se mantiene" cuenta como bajar a proposito (peticion del usuario): en un
 * congelador lo que preocupa es que suba.
 *
 * La exterior lleva la misma flecha y los mismos colores, aunque ahi no sea una
 * alarma sino informacion: naranja = calentando, azul = enfriando. Se lee igual
 * de bien y no hay que aprenderse dos codigos. */
#define TREND_SUBE_CENTI   30   /* +0,30 C sobre la media para decir que sube */
#define TREND_BAJA_CENTI    5   /* +0,05 C para volver a decir que baja */

/* El estado va por sensor: son dos flechas independientes y cada una tiene su
 * propia media y su propio recuerdo de hacia donde iba. */
typedef struct {
    bool  lista;
    float media;
    bool  subiendo;
} tendencia_t;

static void refresh_tendencia(tendencia_t *st, lv_obj_t *flecha,
                               bool has, int16_t centi)
{
    if (!has) {
        st->lista = false;
        st->subiendo = false;
        lv_label_set_text(flecha, "");
        return;
    }

    if (!st->lista) {              /* primera lectura: la media ES el dato */
        st->media = (float)centi;
        st->lista = true;
    } else {
        st->media += ((float)centi - st->media) * 0.02f;
    }

    float diff = (float)centi - st->media;
    if (diff > TREND_SUBE_CENTI)       st->subiendo = true;
    else if (diff < TREND_BAJA_CENTI)  st->subiendo = false;

    lv_label_set_text(flecha, st->subiendo ? LV_SYMBOL_UP : LV_SYMBOL_DOWN);
    lv_obj_set_style_text_color(flecha,
                                st->subiendo ? lv_color_hex(0xFF9800)  /* naranja */
                                             : lv_color_hex(0x4FC3F7), /* azul */
                                0);
}

/* Una de las dos temperaturas que comparten tarjeta. */
static void refresh_temp(lv_obj_t *val, bool has, int16_t centi, bool is_frigo)
{
    char buf[24];
    if (has) {
        int sign = centi < 0 ? -1 : 1;
        int abs_c = centi * sign;
        snprintf(buf, sizeof(buf), "%s%d.%d\xC2\xB0" "C",
                 sign < 0 ? "-" : "", abs_c / 100, (abs_c % 100) / 10);
        lv_label_set_text(val, buf);
        lv_obj_set_style_text_color(val, is_frigo ? color_for_frigo(centi) : COL_TEXT, 0);
    } else {
        lv_label_set_text(val, "--");
        lv_obj_set_style_text_color(val, COL_TEXT_DIM, 0);
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
        /* Grises: 0 = vacio (OK), cualquier cosa por encima = lleno. Mismo
         * criterio que la P4 (ver r1 en ne185.h). */
        bool lleno = d->water_gray > 0;
        led_set(s_led_gray, lleno ? 2 : 0);
        lv_label_set_text(s_lbl_gray, lleno ? "LLENO" : "grises");
        /* Sobre el bloque rojo el texto va en negro; apagado, en gris. */
        lv_obj_set_style_text_color(s_lbl_gray,
                                    lleno ? lv_color_hex(0x000000) : COL_TEXT_DIM, 0);
    } else {
        for (int i = 0; i < 4; i++) led_set(s_led_clean[i], 0);
        led_set(s_led_gray, 0);
        lv_label_set_text(s_lbl_gray, "grises");
        lv_obj_set_style_text_color(s_lbl_gray, COL_TEXT_DIM, 0);
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
    lv_obj_set_style_bg_color(s_bat_dot, col, 0);
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
    static tendencia_t t_frigo, t_ext;
    refresh_temp(s_frigo_val, d.frigo_has_data, d.frigo_temp_centi, true);
    refresh_tendencia(&t_frigo, s_frigo_trend, d.frigo_has_data, d.frigo_temp_centi);
    refresh_temp(s_ext_val, d.exterior_has_data, d.exterior_temp_centi, false);
    refresh_tendencia(&t_ext, s_ext_trend, d.exterior_has_data, d.exterior_temp_centi);

    if (s_gps) {
        /* CADUCA con el enlace, igual que los numeros. El estado del GPS lo
         * manda la P4; si la P4 deja de hablar, el ultimo valor recibido se
         * queda congelado en el modelo y el icono seguiria VERDE diciendo que
         * hay posicion sin que haya ni comunicacion. Mismo criterio que el
         * punto de conexion: sin dato fresco, gris.
         *
         * Es el mismo fallo que gps.c ya evita en la P4 con su caducidad de
         * 5 s; faltaba aplicarlo en el lado que recibe. */
        uint32_t ms = (uint32_t)(esp_timer_get_time() / 1000);
        bool fresco = d.has_data && (ms - d.last_update_ms < CONN_TIMEOUT_MS);
        uint32_t c = !fresco             ? 0x666666    /* sin enlace */
                   : (d.gps_estado == 2) ? 0x4CD964    /* fijado  */
                   : (d.gps_estado == 1) ? 0xFF9800    /* buscando */
                                         : 0x666666;   /* la P4 no ve el GPS */
        lv_obj_set_style_text_color(s_gps, lv_color_hex(c), 0);
    }
    lv_label_set_text(s_frigo_fan, frigo_secondary);
    refresh_aguas(&d);
    update_conn_dots(&d);
}

/* Una tarjeta vacia con su borde de color, su punto de enlace y su titulo
 * arriba a la izquierda. El contenido lo pone cada cual. */
static lv_obj_t *make_card(lv_obj_t *grid, lv_color_t border, const char *titulo,
                            uint8_t col, uint8_t span, uint8_t row, lv_obj_t **dot_out,
                            bool titulo_centrado, const lv_font_t *fuente_titulo)
{
    lv_obj_t *card = lv_obj_create(grid);
    lv_obj_set_grid_cell(card, LV_GRID_ALIGN_STRETCH, col, span, LV_GRID_ALIGN_STRETCH, row, 1);
    lv_obj_set_style_bg_color(card, COL_CARD_BG_TOP, 0);
    lv_obj_set_style_bg_grad_color(card, COL_CARD_BG_BOT, 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, border, 0);
    lv_obj_set_style_border_width(card, 2, 0);
    lv_obj_set_style_radius(card, 10, 0);
    lv_obj_set_style_pad_all(card, 8, 0);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    if (dot_out) {
        lv_obj_t *dot = lv_obj_create(card);
        lv_obj_set_size(dot, 12, 12);
        lv_obj_set_style_bg_color(dot, COL_CONN_NONE, 0);
        lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(dot, 0, 0);
        lv_obj_set_style_radius(dot, 6, 0);
        lv_obj_set_style_pad_all(dot, 0, 0);
        lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
        lv_obj_align(dot, LV_ALIGN_TOP_RIGHT, 0, 2);
        *dot_out = dot;
    }

    lv_obj_t *t = lv_label_create(card);
    lv_label_set_text(t, titulo);
    lv_obj_set_style_text_color(t, border, 0);
    lv_obj_set_style_text_font(t, fuente_titulo, 0);
    /* Los titulos van centrados: pegados a la esquina se perdian, y con el de
     * bateria coronando su dibujo los de abajo desalineados cantaban. */
    lv_obj_align(t, titulo_centrado ? LV_ALIGN_TOP_MID : LV_ALIGN_TOP_LEFT, 0, 0);
    return card;
}

/* Etiqueta pequena + valor grande, uno al lado del otro. La usan la tarjeta de
 * temperaturas y la bateria del motor. */
static lv_obj_t *make_fila_dato(lv_obj_t *padre, const char *etiqueta,
                                 const lv_font_t *fuente_val, lv_coord_t y)
{
    lv_obj_t *l = lv_label_create(padre);
    lv_label_set_text(l, etiqueta);
    lv_obj_set_style_text_color(l, COL_TEXT_ESCALA, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_20, 0);
    lv_obj_align(l, LV_ALIGN_TOP_LEFT, 0, y + 3);

    lv_obj_t *v = lv_label_create(padre);
    lv_label_set_text(v, "--");
    lv_obj_set_style_text_color(v, COL_TEXT, 0);
    lv_obj_set_style_text_font(v, fuente_val, 0);
    lv_obj_align(v, LV_ALIGN_TOP_RIGHT, 0, y);
    return v;
}

void view_info_create(lv_obj_t *parent)
{
    /* Resolucion logica LANDSCAPE 480x320 (ver esp_bsp.c:390-396).
     *
     * DOS columnas y DOS filas, con la bateria ocupando la fila de arriba
     * entera. La de arriba pesa mas (3 contra 2) porque ahi va el dibujo de la
     * bateria con su relleno, que es lo que se mira. */
    static lv_coord_t col_dsc[] = {LV_GRID_FR(1), LV_GRID_FR(1), LV_GRID_TEMPLATE_LAST};
    static lv_coord_t row_dsc[] = {LV_GRID_FR(3), LV_GRID_FR(2), LV_GRID_TEMPLATE_LAST};

    lv_obj_t *grid = lv_obj_create(parent);
    lv_obj_set_size(grid, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_color(grid, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(grid, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 4, 0);
    lv_obj_set_style_pad_gap(grid, 4, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_grid_dsc_array(grid, col_dsc, row_dsc);

    /* --- Bateria: toda la franja de arriba -------------------------------- */
    s_bat_card = /* La de bateria manda, y su titulo tambien: 24 contra los 20 de las de
     * abajo. */
    make_card(grid, COL_BORDER_BAT, "BATERIA", 0, 2, 0, &s_bat_dot, true,
                           &lv_font_montserrat_24);

    /* El dibujo va CENTRADO en la tarjeta y es el protagonista: los voltios y
     * amperios a su izquierda, la bateria del motor a su derecha. El +8 baja el
     * conjunto lo que ocupa el titulo, para que quede centrado a la vista y no
     * solo en la cuenta. */
    lv_obj_t *dib = make_bateria_dibujo(s_bat_card);
    lv_obj_align(dib, LV_ALIGN_CENTER, 0, 8);

    /* Numero y UNIDAD van en etiquetas separadas, y no en un solo texto, para que
     * la V y la A no se muevan: el numero se alinea a la DERECHA dentro de un
     * hueco fijo, asi que crece hacia la izquierda y la unidad se queda clavada.
     * Juntos, "9.99 V" y "13.43 V" dejaban la V en sitios distintos y bailaba
     * cada vez que la tension cruzaba una decena. */
    s_bat_volt = lv_label_create(s_bat_card);
    lv_label_set_text(s_bat_volt, "--");
    lv_obj_set_style_text_color(s_bat_volt, COL_TEXT, 0);
    lv_obj_set_style_text_font(s_bat_volt, &lv_font_montserrat_32, 0);
    lv_obj_set_width(s_bat_volt, BAT_NUM_W);
    lv_obj_set_style_text_align(s_bat_volt, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(s_bat_volt, LV_ALIGN_LEFT_MID, BAT_NUM_X, -12);

    lv_obj_t *u_v = lv_label_create(s_bat_card);
    lv_label_set_text(u_v, "V");
    lv_obj_set_style_text_color(u_v, COL_TEXT_ESCALA, 0);
    lv_obj_set_style_text_font(u_v, &lv_font_montserrat_24, 0);
    lv_obj_align(u_v, LV_ALIGN_LEFT_MID, BAT_NUM_X + BAT_NUM_W + 8, -8);

    s_bat_amp = lv_label_create(s_bat_card);
    lv_label_set_text(s_bat_amp, "");
    lv_obj_set_style_text_color(s_bat_amp, COL_TEXT, 0);
    /* Mismo tamano que los voltios: los dos son el dato principal de su lado. */
    lv_obj_set_style_text_font(s_bat_amp, &lv_font_montserrat_32, 0);
    lv_obj_set_width(s_bat_amp, BAT_NUM_W);
    lv_obj_set_style_text_align(s_bat_amp, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(s_bat_amp, LV_ALIGN_LEFT_MID, BAT_NUM_X, 30);

    s_bat_amp_u = lv_label_create(s_bat_card);
    lv_label_set_text(s_bat_amp_u, "A");
    lv_obj_set_style_text_color(s_bat_amp_u, COL_TEXT_ESCALA, 0);
    lv_obj_set_style_text_font(s_bat_amp_u, &lv_font_montserrat_24, 0);
    lv_obj_align(s_bat_amp_u, LV_ALIGN_LEFT_MID, BAT_NUM_X + BAT_NUM_W + 8, 34);

    /* La del motor, a la derecha del todo y mas discreta: es bateria tambien,
     * pero solo se mira cuando el vehiculo no arranca. */
    /* Titulo y valor en una columna centrada, no colocados a mano: asi "MOTOR"
     * queda centrado sobre el numero SEA CUAL SEA su ancho -- y cambia, que no
     * es lo mismo "9.99 V" que "12.66 V". A mano habria que reajustarlo cada
     * vez que el valor cruza una decena. */
    lv_obj_t *col_motor = lv_obj_create(s_bat_card);
    lv_obj_set_size(col_motor, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(col_motor, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col_motor, 0, 0);
    lv_obj_set_style_pad_all(col_motor, 0, 0);
    lv_obj_set_style_pad_row(col_motor, 10, 0);   /* el aire entre los dos */
    lv_obj_set_flex_flow(col_motor, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col_motor, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    lv_obj_clear_flag(col_motor, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_align(col_motor, LV_ALIGN_RIGHT_MID, -18, 6);

    lv_obj_t *aux_tit = lv_label_create(col_motor);
    lv_label_set_text(aux_tit, "MOTOR");
    lv_obj_set_style_text_color(aux_tit, COL_BORDER_AUX, 0);
    lv_obj_set_style_text_font(aux_tit, &lv_font_montserrat_20, 0);

    s_aux_val = lv_label_create(col_motor);
    lv_label_set_text(s_aux_val, "--");
    lv_obj_set_style_text_color(s_aux_val, COL_TEXT, 0);
    /* Mismo tamano que los voltios de la principal: es el otro dato de tension
     * de la tarjeta y no tiene por que leerse peor. */
    lv_obj_set_style_text_font(s_aux_val, &lv_font_montserrat_32, 0);

    /* --- Abajo izquierda: aguas ------------------------------------------- */
    make_water_cell(grid, 0, 1, 1);

    /* --- Abajo derecha: las dos temperaturas juntas ----------------------- */
    lv_obj_t *temp_card = make_card(grid, COL_BORDER_COLD, "TEMPERATURAS", 1, 1, 1, NULL, true,
                                       &lv_font_montserrat_20);
    /* El valor del frigo va en un hueco fijo y la flecha al borde: asi la
     * flecha no se mueve cuando el numero cambia de ancho ("-5.0" contra
     * "-18.0"), igual que con la V y la A de la bateria. */
    s_frigo_val = make_fila_dato(temp_card, "Frigo", &lv_font_montserrat_24, 26);
    lv_obj_set_width(s_frigo_val, TEMP_NUM_W);
    lv_obj_set_style_text_align(s_frigo_val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(s_frigo_val, LV_ALIGN_TOP_RIGHT, -26, 26);

    s_frigo_trend = lv_label_create(temp_card);
    lv_label_set_text(s_frigo_trend, "");
    lv_obj_set_style_text_font(s_frigo_trend, &lv_font_montserrat_20, 0);
    lv_obj_align(s_frigo_trend, LV_ALIGN_TOP_RIGHT, 0, 30);

    s_ext_val   = make_fila_dato(temp_card, "Exterior", &lv_font_montserrat_24, 62);
    lv_obj_set_width(s_ext_val, TEMP_NUM_W);
    lv_obj_set_style_text_align(s_ext_val, LV_TEXT_ALIGN_RIGHT, 0);
    lv_obj_align(s_ext_val, LV_ALIGN_TOP_RIGHT, -26, 62);

    s_ext_trend = lv_label_create(temp_card);
    lv_label_set_text(s_ext_trend, "");
    lv_obj_set_style_text_font(s_ext_trend, &lv_font_montserrat_20, 0);
    lv_obj_align(s_ext_trend, LV_ALIGN_TOP_RIGHT, 0, 66);

    s_frigo_fan = lv_label_create(temp_card);
    lv_label_set_text(s_frigo_fan, "");
    lv_obj_set_style_text_color(s_frigo_fan, COL_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_frigo_fan, &lv_font_montserrat_14, 0);
    lv_obj_align(s_frigo_fan, LV_ALIGN_BOTTOM_RIGHT, 0, 0);

    /* Pastilla de pendientes: encima de todo y FUERA de la rejilla, para no
     * robarle sitio a ninguna tarjeta -- casi siempre no esta. Abajo al centro,
     * que es donde no tapa ningun numero. */
    s_pendientes = lv_label_create(parent);
    lv_obj_add_flag(s_pendientes, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_pendientes, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_label_set_text(s_pendientes, "");
    lv_obj_set_style_bg_color(s_pendientes, lv_color_hex(0xFF9800), 0);
    lv_obj_set_style_bg_opa(s_pendientes, LV_OPA_COVER, 0);
    lv_obj_set_style_text_color(s_pendientes, lv_color_hex(0x000000), 0);
    /* Letra 24 y no 14: con la 14 quedaba tan pequena que se confundia con un
     * rotulo mas de la pantalla, y esto es lo unico que puede impedirte irte a
     * casa con un apunte sin entregar. Tiene que verse de un vistazo desde el
     * asiento, no leerse de cerca. */
    lv_obj_set_style_text_font(s_pendientes, &lv_font_montserrat_24, 0);
    lv_obj_set_style_pad_hor(s_pendientes, 20, 0);
    lv_obj_set_style_pad_ver(s_pendientes, 8, 0);
    lv_obj_set_style_radius(s_pendientes, LV_RADIUS_CIRCLE, 0);
    /* Sombra negra alrededor: la pastilla cae ENCIMA de las tarjetas de aguas y
     * temperaturas, y sin separacion el naranja se pegaba a sus bordes. */
    lv_obj_set_style_border_color(s_pendientes, lv_color_hex(0x000000), 0);
    lv_obj_set_style_border_width(s_pendientes, 3, 0);
    lv_obj_align(s_pendientes, LV_ALIGN_BOTTOM_MID, 0, -6);
    /* Se TOCA cuando hay algo que cerrar: lleva derecho a la lista. Un aviso
     * que solo avisa obliga a acordarse de a donde hay que ir. */
    lv_obj_add_flag(s_pendientes, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(s_pendientes, 10);
    lv_obj_add_event_cb(s_pendientes, pendientes_click_cb, LV_EVENT_CLICKED, NULL);
    /* La pantalla de registros puede haberse creado antes y haber contado ya lo
     * que quedo abierto; sin esto, ese aviso no saldria hasta el primer cambio. */
    pendientes_aplicar(NULL);

    /* Indicador del GPS de la P4. Arriba a la izquierda, justo detras del punto
     * de conexion: la cabecera de la tarjeta de bateria lleva el titulo
     * CENTRADO, asi que ese hueco esta libre y no le quita sitio a ningun dato.
     *
     * Tres colores, los mismos que en la P4: gris no llega nada, naranja
     * buscando, verde posicion fijada. Un GPS recien encendido tarda un par de
     * minutos, y ver "buscando" en vez de "no hay" evita ir a mirar el cable
     * cuando lo unico que hay que hacer es esperar. */
    s_gps = lv_label_create(parent);
    lv_obj_add_flag(s_gps, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_label_set_text(s_gps, LV_SYMBOL_GPS);
    /* Rejilla 4 de margen + tarjeta 2 de borde y 8 de relleno: el contenido de
     * la tarjeta de bateria empieza en la pantalla a (14, 14). El icono va casi
     * pegado a esa esquina porque AHI NO HAY NADA -- el punto de conexion de
     * esta tarjeta esta arriba a la DERECHA (make_card lo alinea TOP_RIGHT; el
     * de la izquierda es el de la tarjeta de aguas) y el titulo va centrado.
     * Antes estaba en y=1 con letra 14, por encima del borde de la tarjeta. */
    lv_obj_set_style_text_font(s_gps, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_gps, lv_color_hex(0x666666), 0);
    lv_obj_align(s_gps, LV_ALIGN_TOP_LEFT, 14, 11);

    s_refresh_timer = lv_timer_create(refresh_cb, 500, NULL);
}

/* --- Pastilla de pendientes ------------------------------------------------
 *
 * Llega desde la tarea del repartidor, no desde LVGL, asi que el trabajo real
 * se aplaza con lv_async_call: tocar un widget desde otra tarea corrompe la
 * lista de objetos y el fallo aparece mucho despues y en otro sitio. */
static size_t s_pend_valor;
static size_t s_sin_cerrar;

/* UNA sola pastilla para las dos cosas, y no dos: son avisos distintos pero
 * comparten el unico hueco de la pantalla donde no tapan un numero. Dos
 * pastillas se pisarian, y moverlas de sitio segun cual haya se ve peor que
 * leerlas juntas. */
static void pendientes_aplicar(void *arg)
{
    (void)arg;
    if (!s_pendientes) return;
    if (s_pend_valor == 0 && s_sin_cerrar == 0) {
        lv_obj_add_flag(s_pendientes, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    /* Primero lo que pide algo de ti (cerrar un apunte); despues lo que se
     * arregla solo en cuanto la P4 aparezca. */
    /* La flecha solo cuando el toque lleva a algun sitio: "sin enviar" no se
     * arregla tocando nada, se arregla encendiendo la P4. */
    if (s_sin_cerrar && s_pend_valor) {
        lv_label_set_text_fmt(s_pendientes, "%u sin cerrar - %u sin enviar  >",
                              (unsigned)s_sin_cerrar, (unsigned)s_pend_valor);
    } else if (s_sin_cerrar) {
        lv_label_set_text_fmt(s_pendientes, "%u sin cerrar  >", (unsigned)s_sin_cerrar);
    } else {
        lv_label_set_text_fmt(s_pendientes, "%u sin enviar", (unsigned)s_pend_valor);
    }
    lv_obj_clear_flag(s_pendientes, LV_OBJ_FLAG_HIDDEN);
    lv_obj_align(s_pendientes, LV_ALIGN_BOTTOM_MID, 0, -6);
}

static void pendientes_click_cb(lv_event_t *e)
{
    (void)e;
    if (s_sin_cerrar == 0) return;   /* "sin enviar" no lleva a ningun sitio */
    nav_ir_a_sin_cerrar();
}

void view_info_set_pendientes(size_t pendientes)
{
    s_pend_valor = pendientes;
    lv_async_call(pendientes_aplicar, NULL);
}

void view_info_set_sin_cerrar(size_t sin_cerrar)
{
    /* Esta llega DESDE LVGL (la pantalla de registros), pero se aplaza igual:
     * asi las dos entradas hacen lo mismo y no hay que acordarse de cual es
     * cual el dia que se toque. */
    s_sin_cerrar = sin_cerrar;
    lv_async_call(pendientes_aplicar, NULL);
}
