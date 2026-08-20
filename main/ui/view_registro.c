/* view_registro.c - Menu de registros con iconos (Fase 2, ampliado).
 *
 * Rejilla de 5 categorias (iconos del set built-in de LVGL, sin assets de
 * imagen) repartida 3+2 a pantalla completa, cada una con su color. Cada una
 * abre su formulario a pantalla completa con boton de volver.
 *
 * Tocar un campo NO saca el teclado aqui: abre el editor a pantalla completa
 * de entry_screen.c (valor en letra 40 + teclado de 240 px).
 *
 * Nada de esto envia datos todavia -- todos los "Guardar" (y los botones
 * de Iniciar/Finalizar viaje) solo loguean. El envio real a la P4
 * (mini_cmd_t nuevo + receptor en ~/joint/victron) es la Fase 4,
 * deliberadamente fuera de este repo. Inicio/fin de viaje es el primer
 * candidato cuando se abra esa fase (pedido explicito del usuario: "que
 * mande el de 3.5 porque es mas comodo").
 */
#include "view_registro.h"
#include "entry_screen.h"
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


static lv_obj_t *s_grid;
static lv_obj_t *s_forms[CAT_COUNT];

/* --- Reparto 3+2 del menu, a pantalla completa (480x320 apaisada) ---------
 *
 * Los tamanos van en pixeles y no en porcentaje a proposito: en LVGL el hueco
 * entre celdas (pad_gap) NO se descuenta del porcentaje, asi que tres celdas
 * al 33% + dos huecos se salen del ancho y la tercera baja de fila.
 *
 * Si algun dia cambia la resolucion, se recalculan desde display.h:
 *   ancho de 3 = (480 - 2*PAD - 2*GAP) / 3      alto = (320 - 2*PAD - GAP) / 2
 */
#define MENU_PAD     10
#define MENU_GAP     10
#define MENU_TILE_W3 146
#define MENU_TILE_W2 225
#define MENU_TILE_H  145

/* Un color por categoria: fondos CLAROS y vivos con el contenido en NEGRO.
 *
 * La primera version usaba la familia Material 800 (oscura) con texto blanco y
 * se veia apagada en la placa: el morado 0x6A1B9A era casi ilegible y el
 * conjunto daba 3,8-6,4:1 de contraste. Invertido, todas pasan de 8:1 -- mas
 * del doble -- que es lo que hace falta con sol de lado dentro de la
 * autocaravana. Contraste de cada una con el negro, calculado sobre la
 * luminancia relativa (WCAG):
 *
 *   azul     0x4FC3F7 -> 10,5:1      naranja  0xFFA726 -> 10,7:1
 *   verde    0x66BB6A ->  8,8:1      turquesa 0x4DB6AC ->  8,6:1
 *   morado   0xCE93D8 ->  8,8:1
 */
#define COL_VIAJE         0x4FC3F7   /* azul     */
#define COL_REPOSTAJE     0x66BB6A   /* verde    */
#define COL_PEAJE         0xCE93D8   /* morado   */
#define COL_BOMBONA       0xFFA726   /* naranja  */
#define COL_MANTENIMIENTO 0x4DB6AC   /* turquesa */

/* Contenido de las tarjetas: negro puro, que es lo que da el maximo contraste
 * sobre los fondos claros de arriba. */
#define COL_TILE_FG       0x000000

/* Rotulos de campo ("Importe", "Litros"...) sobre el fondo negro del
 * formulario. Estaban en 0x888888, un gris medio que daba 5,9:1 y en la placa
 * apenas se leia. Subido a casi blanco: 14:1. Se queda un punto por debajo del
 * blanco puro del VALOR para que siga notandose cual es el rotulo y cual el
 * dato, jerarquia que refuerza el tamano (16 el rotulo, 24 el valor). */
#define COL_LABEL         0xDDDDDD

/* Botones de accion, mismo criterio: claros con texto negro.
 *   verde  0x66BB6A -> 8,8:1     rojo 0xE57373 -> 7,0:1
 * El rojo se queda algo por debajo del resto a proposito: bajarlo mas lo
 * volveria rosa palido y "Finalizar viaje" tiene que LEERSE como rojo. */
#define COL_ACCION_OK     0x66BB6A
#define COL_ACCION_STOP   0xE57373

static lv_obj_t *s_mant_tipo_dd;
static lv_obj_t *s_mant_km_ta;

static lv_obj_t *s_bombona_cuantas_bm;
static lv_obj_t *s_bombona_precio_ta;
static lv_obj_t *s_bombona_currency_dd;

static lv_obj_t *s_repo_importe_ta;
static lv_obj_t *s_repo_litros_ta;
static lv_obj_t *s_repo_currency_dd;
static lv_obj_t *s_repo_preciolitro_lbl;

/* Monedas de la Europa continental que puede pisar la autocaravana.
 * Por defecto EUR (indice 0). Mismo orden en las dos listas.
 *
 * SOLO ASCII, y no es un descuido: las fuentes Montserrat que trae LVGL se
 * compilan con el rango "0x20-0x7F,0xB0,0x2022" (ver la cabecera de
 * lv_font_montserrat_24.c), o sea ASCII + grado + bullet. El simbolo del euro
 * (U+20AC), la libra, la "z" polaca con barra y la "c" checa con hacek NO
 * existen en la fuente y salian como un cuadrado vacio. Por eso "zl" y "Kc" van
 * sin diacriticos y EUR/GBP se quedan solo con el codigo.
 *
 * Vale igual para el resto de la interfaz: nada de acentos ni "n" con virgulilla
 * en los textos que se pintan, o apareceran cuadrados. */
#define CURRENCY_OPTIONS \
    "EUR\n" "GBP\n" "CHF Fr\n" "SEK kr\n" \
    "NOK kr\n" "DKK kr\n" "PLN zl\n" "CZK Kc\n" \
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

void view_registro_reset(void)
{
    /* s_grid puede no existir todavia si se llamase antes de crear la vista. */
    if (!s_grid) return;
    entry_screen_close();
    show_grid();
}

/* === Edicion de campos (editor a pantalla completa) ===================== */

/* Tocar un campo abre el editor a PANTALLA COMPLETA (entry_screen.c) en vez de
 * sacar el teclado pequeno tapando el formulario.
 *
 * Se engancha a CLICKED y no a FOCUSED: al volver del editor el campo conserva
 * el foco, asi que con FOCUSED el segundo toque sobre el mismo campo no
 * volveria a abrirlo.
 *
 * El rotulo viaja en el user_data del textarea (siempre un literal, vive toda la
 * ejecucion); el user_data del evento lleva si el campo es numerico. */
static void ta_click_cb(lv_event_t *e)
{
    lv_obj_t *ta = lv_event_get_target(e);
    bool numeric = (bool)(uintptr_t)lv_event_get_user_data(e);
    entry_screen_open(ta, (const char *)lv_obj_get_user_data(ta), numeric);
}

/* Fila de campo que SE ESTIRA para repartirse el hueco sobrante del formulario.
 *
 * Antes cada fila media 52 px fijos y el resto de la pantalla quedaba negro: el
 * formulario de peaje (un solo campo) desperdiciaba 170 de los 320 px. Con
 * flex_grow las filas se reparten lo que sobre, asi que un formulario corto sale
 * con filas grandes y uno largo (bombona, 4 campos) las deja en su tamano
 * normal. La altura minima evita que se aplasten si algun dia se anaden campos.
 *
 * Dentro va en columna centrada: rotulo arriba, valor debajo. Asi el contenido
 * queda en el medio de la fila por alta que sea, en vez de pegado al borde. */
#define FIELD_MIN_H  56
#define FIELD_TA_H   40

/* La cabecera crece de 34 a 48 para que quepa el boton de Volver en pastilla.
 * Los 14 px extra los ceden las filas de campo, que son elasticas. */
#define HEADER_H     48

static lv_obj_t *make_field_row(lv_obj_t *parent)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_width(cont, lv_pct(100));
    lv_obj_set_height(cont, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(cont, FIELD_MIN_H, 0);
    lv_obj_set_flex_grow(cont, 1);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 2, 0);
    lv_obj_set_style_pad_row(cont, 2, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_CENTER);
    return cont;
}

static lv_obj_t *make_field(lv_obj_t *parent, const char *label_text,
                             const char *placeholder, bool numeric)
{
    lv_obj_t *cont = make_field_row(parent);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

    lv_obj_t *ta = lv_textarea_create(cont);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, placeholder);
    lv_obj_set_size(ta, lv_pct(100), FIELD_TA_H);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_24, 0);
    if (numeric) {
        lv_textarea_set_accepted_chars(ta, "0123456789.");
    }
    lv_obj_set_user_data(ta, (void *)label_text);
    lv_obj_add_event_cb(ta, ta_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)numeric);

    return ta;
}

/* Campo de importe: numero + selector de moneda (por defecto EUR, pero
 * contempla otras monedas de la Europa continental para cuando se viaje
 * fuera de la zona euro). Devuelve la textarea; *dd_out (si no es NULL)
 * se rellena con el dropdown de moneda por si hace falta leerlo luego. */
static lv_obj_t *make_money_field(lv_obj_t *parent, const char *label_text,
                                   lv_obj_t **dd_out)
{
    lv_obj_t *cont = make_field_row(parent);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

    /* Sub-fila para poner numero y moneda uno al lado del otro dentro de la
     * columna centrada de make_field_row(). */
    lv_obj_t *row = lv_obj_create(cont);
    lv_obj_set_size(row, lv_pct(100), FIELD_TA_H);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *ta = lv_textarea_create(row);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, "0.00");
    lv_obj_set_size(ta, lv_pct(62), FIELD_TA_H);
    lv_obj_align(ta, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_24, 0);
    lv_textarea_set_accepted_chars(ta, "0123456789.");
    lv_obj_set_user_data(ta, (void *)label_text);
    lv_obj_add_event_cb(ta, ta_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)true);

    lv_obj_t *dd = lv_dropdown_create(row);
    lv_dropdown_set_options(dd, CURRENCY_OPTIONS);
    lv_obj_set_size(dd, lv_pct(36), FIELD_TA_H);
    lv_obj_align(dd, LV_ALIGN_RIGHT_MID, 0, 0);
    if (dd_out) *dd_out = dd;

    return ta;
}

/* Selector de pocas opciones excluyentes (p.ej. cuantas bombonas: 1 o 2).
 * Botonera en vez de desplegable: con dos opciones, un desplegable son dos
 * toques y una lista que tapa la pantalla; asi es un toque directo y con
 * botones grandes, que es lo que hace falta con la autocaravana en marcha.
 * 'map' termina en "" (lo exige lv_btnmatrix). Deja marcada la primera. */
static lv_obj_t *make_choice_row(lv_obj_t *parent, const char *label_text,
                                  const char *map[])
{
    lv_obj_t *cont = make_field_row(parent);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

    lv_obj_t *bm = lv_btnmatrix_create(cont);
    lv_btnmatrix_set_map(bm, map);
    lv_obj_set_size(bm, lv_pct(100), FIELD_TA_H + 8);
    lv_obj_set_style_text_font(bm, &lv_font_montserrat_24, 0);
    lv_obj_set_style_bg_opa(bm, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(bm, 0, 0);
    lv_obj_set_style_pad_all(bm, 0, 0);
    lv_btnmatrix_set_btn_ctrl_all(bm, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(bm, true);
    lv_btnmatrix_set_btn_ctrl(bm, 0, LV_BTNMATRIX_CTRL_CHECKED);
    return bm;
}

/* Fila con desplegable, en el mismo formato elastico que los demas campos.
 * Antes el "Tipo" del mantenimiento se montaba a mano con letra del 14 y se
 * quedaba enano al lado de los campos nuevos. */
static lv_obj_t *make_dropdown_row(lv_obj_t *parent, const char *label_text,
                                    const char *options)
{
    lv_obj_t *cont = make_field_row(parent);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

    lv_obj_t *dd = lv_dropdown_create(cont);
    lv_dropdown_set_options(dd, options);
    lv_obj_set_size(dd, lv_pct(100), FIELD_TA_H);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_24, 0);
    return dd;
}

static lv_obj_t *make_readonly_row(lv_obj_t *parent, const char *label_text)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), 42);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 2, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *val = lv_label_create(cont);
    lv_label_set_text(val, "--");
    lv_obj_set_style_text_color(val, lv_color_hex(0xFFFFFF), 0);
    lv_obj_set_style_text_font(val, &lv_font_montserrat_24, 0);
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
    lv_obj_set_size(row, lv_pct(100), HEADER_H);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    /* Volver en forma de pastilla, con el color de la categoria en el borde y
     * en el texto. Antes eran 80x32 en gris sobre negro: se perdia. Al pulsar
     * se invierte (fondo del color, texto blanco) para que se note el toque. */
    lv_obj_t *back = lv_btn_create(row);
    lv_obj_set_size(back, 128, 42);
    lv_obj_set_style_radius(back, 21, 0);
    lv_obj_set_style_bg_color(back, lv_color_hex(0x1E1E1E), 0);
    lv_obj_set_style_border_width(back, 2, 0);
    lv_obj_set_style_border_color(back, color, 0);
    lv_obj_set_style_bg_color(back, color, LV_STATE_PRESSED);
    lv_obj_set_style_border_color(back, color, LV_STATE_PRESSED);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back, back_click_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT "  Volver");
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(back_lbl, color, 0);
    /* Al pulsar el fondo se vuelve del color (claro), asi que el texto pasa a
     * NEGRO, no a blanco: sobre estos tonos vivos el blanco se pierde. */
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(COL_TILE_FG), LV_STATE_PRESSED);
    lv_obj_center(back_lbl);

    /* Centrado en la fila, no respecto al hueco que deja el boton: asi el
     * titulo cae en el eje de la pantalla. Cabe sin pisar el boton -- el mas
     * largo, "MANTENIMIENTO", ocupa ~180 px centrados en 464, o sea empieza
     * en el 142, y el boton acaba en el 128. */
    lv_obj_t *t = lv_label_create(row);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, color, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_22, 0);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, 0);
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
    lv_obj_set_size(btn, lv_pct(100), 50);
    lv_obj_set_style_bg_color(btn, lv_color_hex(COL_ACCION_OK), 0);
    lv_obj_set_style_bg_color(btn, lv_color_darken(lv_color_hex(COL_ACCION_OK), LV_OPA_30),
                              LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, user_data);
    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TILE_FG), 0);
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

/* Boton grande de la pantalla de viaje: se estira para llenar el formulario.
 * Al fijar bg_color propio se pierde el realce de pulsado de LVGL, asi que se
 * oscurece a mano (mismo criterio que los iconos del menu). */
static void make_viaje_button(lv_obj_t *form, const char *text, uint32_t color,
                               lv_event_cb_t cb)
{
    lv_obj_t *btn = lv_btn_create(form);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_height(btn, LV_SIZE_CONTENT);
    lv_obj_set_style_min_height(btn, 70, 0);
    lv_obj_set_flex_grow(btn, 1);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(btn, lv_color_darken(lv_color_hex(color), LV_OPA_30),
                              LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TILE_FG), 0);
    lv_obj_center(lbl);
}

static void build_viaje(lv_obj_t *form)
{
    add_header(form, "VIAJE", lv_color_hex(COL_VIAJE));

    /* Sin campo de coordenada GPS manual aqui a proposito: cuando exista
     * el GPS real (Fase 4 + modulo GPS de la P4) la posicion de
     * inicio/fin se capturaria sola en el instante del toque, no tiene
     * sentido pedirla a mano para una accion pensada como "un solo toque". */
    lv_obj_t *msg = lv_label_create(form);
    lv_label_set_text(msg, "Inicio y fin de viaje de la P4, desde aqui.");
    lv_obj_set_style_text_color(msg, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(msg, lv_pct(100));

    /* Los dos botones se reparten TODO lo que sobra (flex_grow), en vez de
     * quedarse en la altura por defecto y dejar medio formulario en negro.
     * Salen de ~95 px cada uno: son las dos acciones mas usadas de la pantalla
     * y las que hay que acertar de un toque con el vehiculo parando. */
    make_viaje_button(form, LV_SYMBOL_PLAY "  Iniciar viaje", COL_ACCION_OK,
                      viaje_iniciar_cb);
    make_viaje_button(form, LV_SYMBOL_STOP "  Finalizar viaje", COL_ACCION_STOP,
                      viaje_finalizar_cb);
}

static void build_repostaje(lv_obj_t *form)
{
    add_header(form, "REPOSTAJE", lv_color_hex(COL_REPOSTAJE));

    /* Sin coordenada GPS ni hora a peticion del usuario (20-ago-2026): tecleadas
     * a mano no aportan nada y estorban en el surtidor. Cuando se abra la Fase 4
     * las pone la P4 al recibir el evento, que ya sabe donde y cuando esta. */
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
    add_header(form, "PEAJE", lv_color_hex(COL_PEAJE));

    /* Sin coordenada GPS ni hora, igual que repostaje: ver comentario alli. */
    make_money_field(form, "Importe", NULL);

    make_save_button(form, "Guardar peaje", save_generic_cb, "peaje");
}

static void build_bombona(lv_obj_t *form)
{
    add_header(form, "BOMBONA", lv_color_hex(COL_BOMBONA));

    /* Solo lo que el aparato no puede saber: cuantas se han comprado y lo que
     * han costado. Coordenada, dia, hora y lugar los rellena la P4 al recibir
     * el evento (Fase 4) -- ella tiene el reloj bueno y no tiene sentido
     * teclearlos a mano en la gasolinera. */
    /* No lleva el segundo 'const' porque lv_btnmatrix_set_map() pide
     * "const char *map[]" y guarda el puntero al array tal cual. */
    static const char *bombona_map[] = { "1", "2", "" };
    s_bombona_cuantas_bm = make_choice_row(form, "Cuantas has comprado",
                                           bombona_map);
    s_bombona_precio_ta  = make_money_field(form, "Precio total",
                                            &s_bombona_currency_dd);

    make_save_button(form, "Guardar bombona", save_generic_cb, "bombona");
}

static void build_mantenimiento(lv_obj_t *form)
{
    add_header(form, "MANTENIMIENTO", lv_color_hex(COL_MANTENIMIENTO));

    /* Sin coordenada GPS, igual que repostaje, peaje y bombona: la posicion y
     * la fecha las pone la P4 al recibir el evento (Fase 4). Aqui solo va lo
     * que el aparato no puede deducir. */
    s_mant_tipo_dd = make_dropdown_row(form, "Tipo",
                                       "Aceite\nFiltro de aceite\nCorrea\nRuedas");
    s_mant_km_ta   = make_field(form, "Km", "0", true);

    make_save_button(form, "Guardar mantenimiento", save_generic_cb, "mantenimiento");
}

/* === Grid principal ======================================================= */

static lv_obj_t *make_icon_button(lv_obj_t *parent, const char *symbol,
                                   const char *label_text, categoria_t idx,
                                   lv_coord_t w, uint32_t color)
{
    lv_obj_t *btn = lv_btn_create(parent);
    lv_obj_set_size(btn, w, MENU_TILE_H);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    /* Al fijar bg_color propio se pierde el realce de pulsado que trae LVGL,
     * y un boton que no reacciona al tocarlo parece roto: se oscurece a mano. */
    lv_obj_set_style_bg_color(btn, lv_color_darken(lv_color_hex(color), LV_OPA_30),
                              LV_STATE_PRESSED);
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_pad_all(btn, 4, 0);
    lv_obj_set_flex_flow(btn, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(btn, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_event_cb(btn, icon_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)idx);

    lv_obj_t *ic = lv_label_create(btn);
    lv_label_set_text(ic, symbol);
    lv_obj_set_style_text_color(ic, lv_color_hex(COL_TILE_FG), 0);
    lv_obj_set_style_text_font(ic, &lv_font_montserrat_40, 0);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TILE_FG), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    /* "MANTENIMIENTO" es mas ancho que su celda: que parta en dos lineas en
     * vez de recortarse. */
    lv_label_set_long_mode(lbl, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(lbl, lv_pct(100));

    return btn;
}

void view_registro_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);
    /* El tema de LVGL mete padding y borde propios en la pantalla. El reparto
     * 3+2 de abajo encaja al pixel (458 de 460 utiles), asi que unos pocos px
     * comidos aqui bajarian la tercera celda de fila. Se anula explicitamente
     * para que la geometria no dependa del tema. */
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_set_style_border_width(parent, 0, 0);

    /* --- Grid de iconos --- */
    s_grid = lv_obj_create(parent);
    lv_obj_set_size(s_grid, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(s_grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_grid, 0, 0);
    lv_obj_clear_flag(s_grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(s_grid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(s_grid, MENU_PAD, 0);
    lv_obj_set_style_pad_gap(s_grid, MENU_GAP, 0);

    /* 3 arriba (146 px) + 2 abajo (225 px). El salto de fila lo hace solo el
     * ROW_WRAP: 3*146 + 2*10 = 458 cabe en los 460 utiles, y la cuarta ya no. */
    make_icon_button(s_grid, LV_SYMBOL_GPS,      "Viaje",         CAT_VIAJE,
                     MENU_TILE_W3, COL_VIAJE);
    make_icon_button(s_grid, LV_SYMBOL_TINT,     "Repostaje",     CAT_REPOSTAJE,
                     MENU_TILE_W3, COL_REPOSTAJE);
    make_icon_button(s_grid, LV_SYMBOL_LIST,     "Peaje",         CAT_PEAJE,
                     MENU_TILE_W3, COL_PEAJE);
    make_icon_button(s_grid, LV_SYMBOL_REFRESH,  "Bombona",       CAT_BOMBONA,
                     MENU_TILE_W2, COL_BOMBONA);
    make_icon_button(s_grid, LV_SYMBOL_SETTINGS, "Mantenimiento", CAT_MANTENIMIENTO,
                     MENU_TILE_W2, COL_MANTENIMIENTO);

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

    /* --- Editor de campo --- */
    /* Editor de campo a pantalla completa. Se crea el ULTIMO a proposito: asi
     * queda por encima de los formularios en el orden de dibujo. */
    entry_screen_init(parent);
}
