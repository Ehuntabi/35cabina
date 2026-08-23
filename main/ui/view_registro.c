/* view_registro.c - Menu de registros con iconos (Fase 2, ampliado).
 *
 * Rejilla 3+3 a pantalla completa (iconos del set built-in de LVGL, sin assets
 * de imagen), cada celda con su color: cinco categorias de registro, que abren
 * su formulario con boton de volver, y una sexta de **Wi-Fi** que salta a la
 * pantalla de ajustes -- antes era el engranaje de la esquina de la vista
 * principal, que se quito para no tener dos puertas a lo mismo.
 *
 * Tocar un campo NO saca el teclado aqui: abre el editor a pantalla completa
 * de entry_screen.c (valor en letra 40 + teclado de 240 px).
 *
 * VIAJE es contextual: sin viaje solo ofrece "Iniciar viaje" (finalizar lo que
 * no ha empezado no significa nada); con viaje en marcha ofrece "Anotar
 * parada" en grande y "Finalizar viaje" pequeno abajo. De ahi cuelgan dos
 * pantallas mas que no tienen casilla propia en el menu: PARADA (donde has
 * parado y que has hecho) y, dentro de ella, SERVICIOS del area.
 *
 * Nada de esto envia datos todavia -- todos los "Guardar" (y los botones
 * de Iniciar/Finalizar viaje) solo loguean. El envio real a la P4
 * (mini_cmd_t nuevo + receptor en ~/joint/victron) es la Fase 4,
 * deliberadamente fuera de este repo. Inicio/fin de viaje es el primer
 * candidato cuando se abra esa fase (pedido explicito del usuario: "que
 * mande el de 3.5 porque es mas comodo").
 */
#include "view_registro.h"
#include "nav.h"
#include "entry_screen.h"
#include "confirm_screen.h"
#include "config_storage.h"
#include "p4_api.h"
#include "viaje_cola.h"
#include "apunte.h"
#include "tilt.h"
#include "reloj.h"
#include "salida.h"
#include "../data_model.h"
#include "esp_log.h"
#include "esp_timer.h"
#include <stdlib.h>
#include <stdio.h>

static const char *TAG = "view_registro";

/* Las cinco primeras son las casillas del menu. Las dos ultimas NO tienen
 * casilla: son pantallas que cuelgan de Viaje (parada) y de parada
 * (servicios). Van en la misma lista porque comparten todo el andamiaje --
 * contenedor oculto, cabecera con Volver, limpieza al salir. */
typedef enum {
    CAT_VIAJE = 0,
    CAT_REPOSTAJE,
    CAT_PEAJE,
    CAT_BOMBONA,
    CAT_MANTENIMIENTO,
    CAT_PARADA,
    CAT_SERVICIOS,
    CAT_VALORACION,
    CAT_COUNT
} categoria_t;

/* Destino del boton Volver de una cabecera: al menu de iconos o a otra
 * pantalla (parada vuelve a viaje, servicios vuelve a parada). */
#define BACK_TO_GRID  (-1)


static lv_obj_t *s_forms[CAT_COUNT];
/* Puesto a true al final de view_registro_create(). view_registro_reset()
 * puede llegar antes de que la vista exista (el carrusel la crea perezosa). */
static bool      s_ui_lista;

/* --- Reparto 3+2 del menu, a pantalla completa (480x320 apaisada) ---------
 *
 * Los tamanos van en pixeles y no en porcentaje a proposito: en LVGL el hueco
 * entre celdas (pad_gap) NO se descuenta del porcentaje, asi que tres celdas
 * al 33% + dos huecos se salen del ancho y la tercera baja de fila.
 *
 * Seis celdas iguales en 3+3. Si algun dia cambia la resolucion:
 *   ancho = (480 - 2*PAD - 2*GAP) / 3      alto = (320 - 2*PAD - GAP) / 2
 */
#define MENU_PAD     10
#define MENU_GAP     10
#define MENU_TILE_W  146
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
/* Ajustes Wi-Fi: gris azulado a proposito, para que NO parezca una categoria
 * de registro mas -- no apunta nada del viaje, configura el aparato.
 * 0xB0BEC5 -> 11,4:1 con el negro. */
#define COL_AJUSTES       0xB0BEC5   /* gris azulado */

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

static lv_obj_t *s_peaje_importe_ta;
static lv_obj_t *s_peaje_currency_dd;

/* --- Viaje: la pantalla cambia segun haya viaje en marcha o no ------------
 *
 * El estado lo lleva la PROPIA pantalla y se guarda en NVS, asi que un corte
 * de corriente no devuelve el menu a "sin viaje". La P4 sigue siendo la duena
 * del viaje de verdad; esto es solo lo que cree la 35cabina hasta que la
 * Fase 4 abra el canal de vuelta y pueda preguntarselo. */
static bool      s_viaje_activo;
static lv_obj_t *s_viaje_title_lbl;      /* "VIAJE" / "VIAJE EN CURSO" */
static lv_obj_t *s_viaje_msg;
static lv_obj_t *s_viaje_btn_iniciar;
static lv_obj_t *s_viaje_btn_parada;
static lv_obj_t *s_viaje_btn_finalizar;
static lv_obj_t *s_viaje_btn_fin_parada;  /* solo si hay una parada abierta */
/* Copia en memoria de si hay parada abierta, para no leer la NVS cada vez que
 * se entra en la pantalla de Viaje. La NVS manda; esto solo la sigue. */
static bool      s_parada_abierta;
/* Campo oculto donde el editor a pantalla completa deja el destino tecleado. No
 * se ve nunca: el editor necesita un textarea al que volcar, y en la pantalla de
 * Viaje no hay formulario donde ponerlo. */
static lv_obj_t *s_viaje_destino_ta;
static char      s_viaje_destino[24];

/* --- Parada: donde has parado y que has hecho ------------------------------
 * Varias a la vez: en un area sueles vaciar Y llenar en la misma parada. */
#define PARADA_COUNT         6
#define PARADA_IDX_PERNOCTA  3
#define PARADA_IDX_AREA      4
#define PARADA_IDX_CAMPING   5

/* Los tres SITIOS son excluyentes entre si: has parado en un sitio de un tipo,
 * no en dos a la vez. Vaciado y llenado quedan fuera de esta lista a proposito
 * -- son cosas que HACES, y se pueden hacer en cualquiera de los tres. */
static const uint8_t PARADA_LUGARES[] = {
    PARADA_IDX_PERNOCTA, PARADA_IDX_AREA, PARADA_IDX_CAMPING
};
/* "Agua potable" va aparte de "Llenado" porque se puede parar SOLO por eso:
 * una fuente al borde de la carretera no es un vaciado ni un area. */
static const char *const PARADA_OPCIONES[PARADA_COUNT] = {
    "Vaciado", "Llenado", "Agua potable", "Pernocta gratis", "Area", "Camping"
};
/* Nombres cortos para el resumen de la confirmacion, por el mismo motivo que
 * SERV_CORTOS: alli caben ~25 caracteres por linea. */
static const char *const PARADA_CORTOS[PARADA_COUNT] = {
    "Vaciado", "Llenado", "Agua", "Pernocta", "Area", "Camping"
};
static lv_obj_t *s_parada_chk[PARADA_COUNT];
static lv_obj_t *s_parada_precio_row;    /* oculto salvo area o camping */
static lv_obj_t *s_parada_precio_lbl;    /* "Precio por" / "Precio por noche" */

/* Como cobra el sitio. Un camping cobra por NOCHES; un area, segun cual: las
 * hay por noche y las hay por periodos de 24 h desde que entras, y eso hay que
 * decirlo al llegar, que es cuando tienes el cartel delante. */
#define PARADA_COBRO_NOCHE  0
#define PARADA_COBRO_24H    1
static lv_obj_t *s_parada_cobro_bm;      /* solo visible en area */
static lv_obj_t *s_parada_precio_ta;
static lv_obj_t *s_parada_currency_dd;
static lv_obj_t *s_parada_servicios_btn; /* oculto salvo area */

/* Servicios que ofrece el area, en su propia pantalla: las cinco casillas de
 * parada + el precio + estas seis no caben juntas en 320 px de alto. */
/* La ultima no es un servicio: es la puerta a la pantalla de valoracion. */
#define SERV_COUNT           7
#define SERV_IDX_VALORACION  6
static const char *const SERV_OPCIONES[SERV_COUNT] = {
    "Agua potable", "Vaciado grises", "Vaciado WC",
    "Electricidad", "Duchas/WC", "Basura",
    "Valoracion"
};
static lv_obj_t *s_serv_chk[SERV_COUNT];
static lv_obj_t *s_serv_hint;            /* explica que se esta marcando */

/* --- Valoracion del sitio, en pantalla propia -----------------------------
 *
 * Antes era una tira de tres botones que se desplegaba dentro de servicios y
 * habia que apretar las casillas para hacerle sitio. En su propia pantalla hay
 * espacio para botones de un dedo y con color, que es lo que hace falta para
 * acertar sin mirar, y ademas caben debajo las dos pegas del sitio.
 *
 * Verde / ambar / rojo, los mismos tonos claros con texto negro del resto de
 * la interfaz (ver el bloque de colores de arriba). */
#define VALORACION_COUNT 3
static const char *const VALORACION[VALORACION_COUNT] = {
    "Recomendado", "Aceptable", "Sucio"
};
static const uint32_t VALORACION_COL[VALORACION_COUNT] = {
    0x66BB6A, 0xFFA726, 0xE57373
};
static uint8_t   s_val_nota;             /* indice en VALORACION */
static lv_obj_t *s_val_btn[VALORACION_COUNT];
static lv_obj_t *s_val_btn_lbl[VALORACION_COUNT];

/* Las pegas del sitio: no son notas sino cosas que pueden pasar con cualquier
 * nota (un sitio recomendable puede no tener sombra). Por eso son casillas
 * sueltas y no opciones de la nota. */
#define VAL_EXTRA_COUNT 2
static const char *const VAL_EXTRAS[VAL_EXTRA_COUNT] = { "Ruidoso", "Sin sombra" };
static lv_obj_t *s_val_extra_chk[VAL_EXTRA_COUNT];

/* Mantenimiento: varias casillas a la vez, no una opcion. Con el mismo
 * kilometraje puedes haber hecho el aceite Y su filtro. */
#define MANT_COUNT 6
#define MANT_IDX_RUEDAS 5          /* la ultima: es la que despliega el contador */
static const char *const MANT_OPCIONES[MANT_COUNT] = {
    "Aceite", "Filtro aceite", "Filtro aire", "Filtro habitaculo",
    "Correa", "Ruedas"
};
static lv_obj_t *s_mant_chk[MANT_COUNT];
static lv_obj_t *s_mant_ruedas_bm;   /* cuantas ruedas; oculto si no se marcan */
static lv_obj_t *s_mant_km_ta;
static lv_obj_t *s_mant_coste_ta;

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

/* Definidos abajo, junto a los widgets que tocan. */
static void clear_forms(void);
static void valoracion_actualiza_texto(bool marcado);
static void valoracion_reset(void);

/* Definidas abajo, con los menus de la salida. */
static void ocultar_menus(void);
static void volver_al_menu(void);

/* Volver al menu deja los formularios EN BLANCO: se vacian sus campos, casillas
 * y selectores. Como es el unico camino de vuelta (boton Volver, guardado
 * confirmado y salida por gesto pasan todos por aqui), basta con hacerlo en un
 * sitio.
 *
 * A que menu se vuelve depende de si hay salida en marcha y de que tipo: lo
 * decide volver_al_menu(). */
static void show_grid(void)
{
    clear_forms();
    for (int i = 0; i < CAT_COUNT; i++) {
        lv_obj_add_flag(s_forms[i], LV_OBJ_FLAG_HIDDEN);
    }
    volver_al_menu();
}

static void viaje_refresh(void);

static void show_form(int idx)
{
    /* La pantalla de viaje tiene dos caras (con viaje y sin el); se pone al dia
     * aqui para que valga igual venga del menu o de cerrar una parada. */
    if (idx == CAT_VIAJE) viaje_refresh();

    ocultar_menus();
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

static void set_hidden(lv_obj_t *obj, bool hidden)
{
    if (hidden) lv_obj_add_flag(obj, LV_OBJ_FLAG_HIDDEN);
    else        lv_obj_clear_flag(obj, LV_OBJ_FLAG_HIDDEN);
}

/* La tarjeta de Wi-Fi no abre un formulario: salta a la pantalla de ajustes,
 * que vive fuera del carrusel. Antes era el engranaje pequeno de la esquina de
 * la vista principal. */
static void ajustes_click_cb(lv_event_t *e)
{
    (void)e;
    nav_open_ajustes();
}

/* El destino viaja +1 en el user_data para que el 0 (CAT_VIAJE) no se
 * confunda con "sin dato". BACK_TO_GRID llega aqui como 0. */
static void back_click_cb(lv_event_t *e)
{
    int dest = (int)(intptr_t)lv_event_get_user_data(e) - 1;
    if (dest < 0) show_grid();
    else          show_form(dest);
}

void view_registro_reset(void)
{
    if (!s_ui_lista) return;
    entry_screen_close();
    confirm_screen_close();
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

/* Ancho maximo del titulo para no pisar el boton de Volver. Ver add_header(). */
#define HEADER_TITLE_MAX_W  200

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

/* Campo de importe: numero + selector de moneda (por defecto EUR, pero
 * contempla otras monedas de la Europa continental para cuando se viaje
 * fuera de la zona euro). Devuelve la textarea; *dd_out (si no es NULL)
 * se rellena con el dropdown de moneda por si hace falta leerlo luego. */
/* Variante APILADA del campo de importe: la moneda arriba y el numero debajo,
 * los dos a lo ancho y con letra mayor. Solo la usa peaje, que tiene un unico
 * campo y por tanto una fila que se estira hasta ~195 px: sitio de sobra. En
 * repostaje no cabria, porque comparte el alto con litros y precio/litro. */
#define MONEY_BIG_DD_H   48
#define MONEY_BIG_TA_H   58

static lv_obj_t *make_money_field_stacked(lv_obj_t *parent, const char *label_text,
                                           lv_obj_t **dd_out)
{
    lv_obj_t *cont = make_field_row(parent);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

    lv_obj_t *dd = lv_dropdown_create(cont);
    lv_dropdown_set_options(dd, CURRENCY_OPTIONS);
    lv_obj_set_size(dd, lv_pct(100), MONEY_BIG_DD_H);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_32, 0);
    if (dd_out) *dd_out = dd;

    lv_obj_t *ta = lv_textarea_create(cont);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, "0.00");
    lv_obj_set_size(ta, lv_pct(100), MONEY_BIG_TA_H);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_40, 0);
    lv_obj_set_style_text_align(ta, LV_TEXT_ALIGN_CENTER, 0);
    lv_textarea_set_accepted_chars(ta, "0123456789.");
    lv_obj_set_user_data(ta, (void *)label_text);
    lv_obj_add_event_cb(ta, ta_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)true);

    return ta;
}

/* Selector de moneda suelto, a lo ancho y grande. Lo usan peaje y repostaje
 * encima de sus importes. */
static lv_obj_t *make_currency_row(lv_obj_t *parent, const char *label_text)
{
    lv_obj_t *cont = make_field_row(parent);

    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_16, 0);

    lv_obj_t *dd = lv_dropdown_create(cont);
    lv_dropdown_set_options(dd, CURRENCY_OPTIONS);
    lv_obj_set_size(dd, lv_pct(100), MONEY_BIG_DD_H);
    lv_obj_set_style_text_font(dd, &lv_font_montserrat_32, 0);
    return dd;
}

/* Media fila: rotulo pequeno arriba y numero grande debajo. */
static lv_obj_t *make_half_number(lv_obj_t *row, const char *label_text)
{
    lv_obj_t *col = lv_obj_create(row);
    lv_obj_set_size(col, lv_pct(48), lv_pct(100));
    lv_obj_set_style_bg_opa(col, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(col, 0, 0);
    lv_obj_set_style_pad_all(col, 0, 0);
    lv_obj_set_style_pad_row(col, 2, 0);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(col, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    lv_obj_t *lbl = lv_label_create(col);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_LABEL), 0);
    /* 20 y no 16 como el resto de rotulos: en esta fila el rotulo es lo unico
     * que distingue un numero del otro, asi que tiene que leerse de golpe. */
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_20, 0);

    lv_obj_t *ta = lv_textarea_create(col);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_placeholder_text(ta, "0.00");
    lv_obj_set_size(ta, lv_pct(100), MONEY_BIG_TA_H);
    lv_obj_set_style_text_font(ta, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_align(ta, LV_TEXT_ALIGN_CENTER, 0);
    lv_textarea_set_accepted_chars(ta, "0123456789.");
    lv_obj_set_user_data(ta, (void *)label_text);
    lv_obj_add_event_cb(ta, ta_click_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)true);
    return ta;
}

/* Dos numeros en la MISMA linea, cada uno con su rotulo. Para repostaje, que
 * pide importe y litros: puestos uno al lado del otro caben los dos grandes y
 * queda sitio para el precio/litro calculado debajo. */
static void make_dual_number_row(lv_obj_t *parent,
                                  const char *l1, lv_obj_t **ta1_out,
                                  const char *l2, lv_obj_t **ta2_out)
{
    lv_obj_t *cont = make_field_row(parent);
    lv_obj_set_flex_flow(cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(cont, LV_FLEX_ALIGN_SPACE_BETWEEN,
                          LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    if (ta1_out) *ta1_out = make_half_number(cont, l1);
    if (ta2_out) *ta2_out = make_half_number(cont, l2);
}

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

/* Casillas de verificacion en 2x2, MULTIPLE (no excluyentes).
 *
 * Sustituyen al desplegable del mantenimiento: con el mismo kilometraje puedes
 * haber hecho varias cosas a la vez -- el aceite Y su filtro es el caso
 * tipico -- y un desplegable solo dejaba elegir una.
 *
 * Se reparten en dos columnas para que la casilla y su texto sigan siendo
 * grandes; en una sola columna las cuatro no caben sin encoger. */
#define CHK_H      32   /* alto de cada casilla con su texto */
#define CHK_GAP     6
/* La pantalla de servicios usa DOS huecos y los cambia sobre la marcha, porque
 * el sitio da para uno o para el otro segun este el selector de valoracion:
 *
 *   selector recogido -> 48 cabecera + 20 rotulo + rejilla + 12 de huecos, con
 *     224 px libres para la rejilla: cuatro filas a 32 con 26 de separacion son
 *     214, entran holgadas y se ve despejado.
 *   selector desplegado -> se le van 56 px, quedan 168 para la rejilla: a 26 de
 *     separacion se saldria (214), asi que se aprieta a 6 y baja a 154.
 *
 * Apretar SOLO mientras eliges la nota es lo que permite tener las dos cosas:
 * casillas separadas el resto del tiempo, que es cuando se tocan. */
#define SERV_CHK_GAP  26

/* 'gap' es el hueco vertical entre filas de casillas. Mantenimiento y parada
 * van con CHK_GAP (6): ahi cada pixel esta comprometido. La pantalla de
 * servicios, en cambio, solo lleva cabecera y casillas, asi que puede
 * permitirse separarlas y ganar puntería. */
static lv_obj_t *make_check_grid(lv_obj_t *parent, const char *const *options,
                                  uint8_t n, lv_obj_t **out, uint32_t color,
                                  lv_coord_t gap)
{
    lv_obj_t *cont = make_field_row(parent);
    /* Sin rotulo: las cuatro opciones se explican solas y el texto de arriba
     * robaba la altura que necesitaban las casillas, que salian cortadas.
     *
     * La altura minima se reserva a mano para las filas que vayan a hacer
     * falta: con LV_SIZE_CONTENT dentro de una fila elastica, si la fila se
     * queda corta el contenido se recorta en silencio (no hay scroll). */
    uint8_t filas = (uint8_t)((n + 1) / 2);
    lv_obj_set_style_min_height(cont, filas * CHK_H + (filas - 1) * gap + 8, 0);

    lv_obj_t *grid = lv_obj_create(cont);
    lv_obj_set_size(grid, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_opa(grid, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(grid, 0, 0);
    lv_obj_set_style_pad_all(grid, 0, 0);
    lv_obj_set_style_pad_row(grid, gap, 0);
    lv_obj_set_style_pad_column(grid, CHK_GAP, 0);
    lv_obj_clear_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
    lv_obj_set_flex_align(grid, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START);

    for (uint8_t i = 0; i < n; i++) {
        lv_obj_t *cb = lv_checkbox_create(grid);
        lv_checkbox_set_text(cb, options[i]);
        /* Alto explicito: con LV_SIZE_CONTENT cada casilla salia de una altura
         * distinta segun el texto y la fila de abajo se recortaba. */
        lv_obj_set_size(cb, lv_pct(48), CHK_H);
        lv_obj_set_style_text_color(cb, lv_color_hex(0xFFFFFF), 0);
        lv_obj_set_style_text_font(cb, &lv_font_montserrat_20, 0);
        /* Casilla grande: la de serie es diminuta para un dedo en marcha. */
        lv_obj_set_style_width(cb, 28, LV_PART_INDICATOR);
        lv_obj_set_style_height(cb, 28, LV_PART_INDICATOR);
        lv_obj_set_style_bg_color(cb, lv_color_hex(color),
                                  LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_border_color(cb, lv_color_hex(COL_LABEL),
                                      LV_PART_INDICATOR);
        if (out) out[i] = cb;
    }
    return grid;
}

/* Cual de los botones de una botonera esta marcado. lv_btnmatrix_get_selected_btn()
 * NO vale: devuelve el ultimo pulsado, no el que quedo marcado. */
static uint16_t btnmatrix_checked(lv_obj_t *bm, uint16_t count)
{
    for (uint16_t i = 0; i < count; i++) {
        if (lv_btnmatrix_has_btn_ctrl(bm, i, LV_BTNMATRIX_CTRL_CHECKED)) return i;
    }
    return 0;
}

/* Cuantas ruedas hay elegidas ahora mismo (1..4). */
static unsigned ruedas_elegidas(void)
{
    return (unsigned)(btnmatrix_checked(s_mant_ruedas_bm, 4) + 1);
}

/* La casilla de Ruedas lleva el numero elegido pegado ("Ruedas: 4"), para
 * verlo sin bajar la vista al selector. Al desmarcar vuelve a su texto seco. */
static void ruedas_actualiza_texto(bool marcado)
{
    lv_obj_t *cb = s_mant_chk[MANT_IDX_RUEDAS];
    if (!marcado) {
        lv_checkbox_set_text(cb, MANT_OPCIONES[MANT_IDX_RUEDAS]);
        return;
    }
    char buf[20];
    snprintf(buf, sizeof(buf), "%s: %u", MANT_OPCIONES[MANT_IDX_RUEDAS],
             ruedas_elegidas());
    lv_checkbox_set_text(cb, buf);   /* set_text copia la cadena */
}

/* Deja marcado el primer boton de una botonera de opcion unica.
 * lv_btnmatrix_set_btn_ctrl() por si solo NO desmarca los demas: el reparto lo
 * hace el manejador interno del widget al pulsar (make_one_button_checked en
 * lv_btnmatrix.c), no la API. Por eso se limpian todos primero. */
static void btnmatrix_reset(lv_obj_t *bm)
{
    lv_btnmatrix_clear_btn_ctrl_all(bm, LV_BTNMATRIX_CTRL_CHECKED);
    lv_btnmatrix_set_btn_ctrl(bm, 0, LV_BTNMATRIX_CTRL_CHECKED);
}

/* Area y camping son las paradas que se PAGAN: solo ellas sacan el precio y la
 * lista de servicios. Una pernocta gratis no tiene ni lo uno ni lo otro.
 * Aparecen y desaparecen segun las casillas, igual que el contador de ruedas
 * del mantenimiento.
 *
 * Los servicios son el mismo dato con dos lecturas: en un area es lo que
 * OFRECE y en un camping lo que va INCLUIDO en el precio. Misma lista y misma
 * pantalla; lo unico que cambia es el rotulo que la explica. */
static bool parada_es_de_pago(void)
{
    return lv_obj_has_state(s_parada_chk[PARADA_IDX_AREA], LV_STATE_CHECKED) ||
           lv_obj_has_state(s_parada_chk[PARADA_IDX_CAMPING], LV_STATE_CHECKED);
}

/* Como se paga el sitio ahora mismo. El camping siempre por noches; el area,
 * lo que diga su interruptor. */
static uint8_t parada_cobro_actual(void)
{
    if (lv_obj_has_state(s_parada_chk[PARADA_IDX_CAMPING], LV_STATE_CHECKED)) {
        return PARADA_COBRO_NOCHE;
    }
    return (uint8_t)btnmatrix_checked(s_parada_cobro_bm, 2);
}

/* El precio es siempre POR UNIDAD DE ESTANCIA, no el total: es lo que se
 * anuncia a la entrada y lo unico que permite calcular la cuenta cuando la
 * parada acaba dias despues.
 *
 * En un camping la unidad es la noche y no hay nada que elegir, asi que el
 * interruptor se esconde y el rotulo lo dice entero. En un area sale el
 * interruptor y el rotulo se queda en "Precio por", que lo completa el boton
 * marcado: "Precio por [Noche|24 h]". */
static void parada_refresh_extras(void)
{
    bool pago = parada_es_de_pago();
    set_hidden(s_parada_precio_row, !pago);
    set_hidden(s_parada_servicios_btn, !pago);

    /* En un camping siempre se cobra por noches: el selector se esconde -- y los
     * otros dos se reparten su hueco, que la fila es elastica -- y el rotulo lo
     * dice entero. En un area lo dice el boton marcado del selector. */
    bool camping = lv_obj_has_state(s_parada_chk[PARADA_IDX_CAMPING], LV_STATE_CHECKED);
    set_hidden(s_parada_cobro_bm, camping);
    lv_label_set_text(s_parada_precio_lbl, camping ? "Precio por noche" : "Precio");

    /* Titulo del editor a pantalla completa: ahi si cabe entero. Los dos son
     * literales, viven toda la ejecucion y se pueden guardar tal cual. */
    lv_obj_set_user_data(s_parada_precio_ta,
                         (void *)(parada_cobro_actual() == PARADA_COBRO_24H
                                  ? "Precio por 24 h" : "Precio por noche"));
}

static void parada_cobro_cb(lv_event_t *e)
{
    (void)e;
    parada_refresh_extras();
}

/* Vacia lo que depende del SITIO: el precio y los servicios. Al pasar de area a
 * camping (o a pernocta gratis) lo tecleado era de la otra, y arrastrarlo es
 * como acabo el peaje guardandose el importe del anterior. */
static void parada_clear_extras(void)
{
    lv_textarea_set_text(s_parada_precio_ta, "");
    lv_dropdown_set_selected(s_parada_currency_dd, 0);
    btnmatrix_reset(s_parada_cobro_bm);
    for (uint8_t i = 0; i < SERV_COUNT; i++) {
        lv_obj_clear_state(s_serv_chk[i], LV_STATE_CHECKED);
    }
    /* Quitar el estado a mano no dispara VALUE_CHANGED, asi que
     * valoracion_toggle_cb no se entera y hay que deshacer aqui lo que habria
     * hecho el: nota a cero, pegas sin marcar y la casilla con su texto. */
    valoracion_reset();
    valoracion_actualiza_texto(false);
}

/* Vacia TODOS los formularios. Lo llama show_grid(), o sea cada vez que se
 * vuelve al menu de iconos, venga de donde venga.
 *
 * Antes solo se ocultaba el formulario y los datos seguian dentro: al reabrir
 * la categoria te encontrabas el importe del peaje anterior. El riesgo no era
 * teclear de mas, era GUARDAR sin mirar el dato de la vez pasada.
 *
 * La moneda vuelve tambien a EUR (indice 0), decision del usuario del
 * 21-ago-2026: fuera de la zona euro obliga a elegirla en cada apunte, pero
 * evita anotar euros con la moneda del pais anterior aun puesta. */
static void clear_forms(void)
{
    lv_textarea_set_text(s_peaje_importe_ta, "");
    lv_dropdown_set_selected(s_peaje_currency_dd, 0);

    lv_textarea_set_text(s_repo_importe_ta, "");
    lv_textarea_set_text(s_repo_litros_ta, "");
    lv_dropdown_set_selected(s_repo_currency_dd, 0);
    /* Despues de vaciar los dos numeros: set_text avisa a repo_recalc_cb, que
     * ya lo deja en "--", pero no depender de ese orden sale mas barato que
     * razonarlo cada vez que se toque el formulario. */
    lv_label_set_text(s_repo_preciolitro_lbl, "--");

    lv_textarea_set_text(s_bombona_precio_ta, "");
    lv_dropdown_set_selected(s_bombona_currency_dd, 0);
    btnmatrix_reset(s_bombona_cuantas_bm);

    lv_textarea_set_text(s_mant_km_ta, "");
    lv_textarea_set_text(s_mant_coste_ta, "");
    for (uint8_t i = 0; i < MANT_COUNT; i++) {
        lv_obj_clear_state(s_mant_chk[i], LV_STATE_CHECKED);
    }
    btnmatrix_reset(s_mant_ruedas_bm);
    /* Quitar el estado a mano NO dispara VALUE_CHANGED, asi que el texto de la
     * casilla ("Ruedas: 4") y el ocultado de la fila del contador hay que
     * rehacerlos aqui; si no, ruedas_toggle_cb no se entera. */
    ruedas_actualiza_texto(false);
    lv_obj_add_flag(lv_obj_get_parent(s_mant_ruedas_bm), LV_OBJ_FLAG_HIDDEN);

    for (uint8_t i = 0; i < PARADA_COUNT; i++) {
        lv_obj_clear_state(s_parada_chk[i], LV_STATE_CHECKED);
    }
    parada_clear_extras();
    /* Vuelve a esconder el precio y el boton de servicios. NO se limpia el
     * viaje en curso: eso no es un dato del formulario, es el estado del
     * aparato y solo lo cambian Iniciar/Finalizar. */
    parada_refresh_extras();
}

/* Al marcar/desmarcar Ruedas aparece o se esconde el contador de cuantas.
 * Se oculta la FILA entera (el padre), no solo la botonera: si no, el rotulo
 * "Cuantas ruedas" se quedaria colgado y la fila seguiria ocupando alto. */
static void ruedas_toggle_cb(lv_event_t *e)
{
    bool marcado = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    lv_obj_t *fila = lv_obj_get_parent(s_mant_ruedas_bm);
    if (marcado) lv_obj_clear_flag(fila, LV_OBJ_FLAG_HIDDEN);
    else         lv_obj_add_flag(fila, LV_OBJ_FLAG_HIDDEN);
    ruedas_actualiza_texto(marcado);
}

/* OJO con lv_btnmatrix: manda LV_EVENT_VALUE_CHANGED YA AL PRESIONAR
 * (lv_btnmatrix.c:462), y la marca CHECKED no se aplica hasta el SOLTAR
 * (lv_btnmatrix.c:503-514), que manda un segundo VALUE_CHANGED.
 *
 * La primera version escondia el selector desde VALUE_CHANGED: se ocultaba al
 * tocar, el dedo se levantaba sobre un objeto ya oculto, el RELEASED nunca
 * llegaba a la botonera y la eleccion NO se aplicaba nunca. Parecia que el
 * selector "no funcionaba".
 *
 * Por eso van separados: el texto se refresca en VALUE_CHANGED (al presionar
 * saldra el valor viejo, pero el segundo aviso del soltar lo corrige) y el
 * cierre se hace en RELEASED. Se puede confiar en ese orden: en
 * lv_event.c:452 el manejador propio del widget corre ANTES que los callbacks
 * anadidos con lv_obj_add_event_cb, asi que al llegar aqui la marca ya esta
 * puesta. */
static void ruedas_num_cb(lv_event_t *e)
{
    (void)e;
    ruedas_actualiza_texto(true);
}

/* Al soltar, el selector ha cumplido: se esconde y el dato se queda a la vista
 * en la propia casilla ("Ruedas: 4"). Asi el formulario recupera la altura y
 * no hay que deslizar para llegar a Guardar.
 *
 * Para cambiarlo se desmarca Ruedas y se vuelve a marcar. Es un caso raro y
 * evita un boton de "editar" ocupando sitio de forma permanente. */
static void ruedas_release_cb(lv_event_t *e)
{
    (void)e;
    ruedas_actualiza_texto(true);
    lv_obj_add_flag(lv_obj_get_parent(s_mant_ruedas_bm), LV_OBJ_FLAG_HIDDEN);
}

/* === Valoracion del sitio ================================================= */

static const char *valoracion_elegida(void)
{
    return VALORACION[s_val_nota];
}

/* Marcada, la casilla de servicios muestra la NOTA en vez de la palabra
 * "Valoracion": asi se lee de un vistazo cual has puesto. El nombre completo no
 * cabria -- "Valoracion: Recomendado" son 23 caracteres y en media rejilla
 * entran ~16. Las pegas (ruidoso, sin sombra) no caben ahi; salen en el resumen
 * de la confirmacion, antes de guardar. */
static void valoracion_actualiza_texto(bool marcado)
{
    lv_checkbox_set_text(s_serv_chk[SERV_IDX_VALORACION],
                         marcado ? valoracion_elegida()
                                 : SERV_OPCIONES[SERV_IDX_VALORACION]);
}

/* La elegida va a todo color y con la marca de visto; las otras dos, apagadas.
 * Con solo el borde no se distinguia de lejos y con solo el color tampoco: dos
 * senales a la vez es lo que hace que se lea de una ojeada y con sol de lado. */
static void valoracion_pinta(void)
{
    for (uint8_t i = 0; i < VALORACION_COUNT; i++) {
        bool sel = (i == s_val_nota);
        lv_obj_set_style_bg_opa(s_val_btn[i], sel ? LV_OPA_COVER : LV_OPA_40, 0);
        lv_obj_set_style_border_width(s_val_btn[i], sel ? 4 : 0, 0);
        lv_label_set_text_fmt(s_val_btn_lbl[i], "%s%s",
                              sel ? LV_SYMBOL_OK "  " : "", VALORACION[i]);
    }
}

static void valoracion_reset(void)
{
    s_val_nota = 0;
    for (uint8_t i = 0; i < VAL_EXTRA_COUNT; i++) {
        lv_obj_clear_state(s_val_extra_chk[i], LV_STATE_CHECKED);
    }
    valoracion_pinta();
}

static void valoracion_nota_cb(lv_event_t *e)
{
    s_val_nota = (uint8_t)(uintptr_t)lv_event_get_user_data(e);
    valoracion_pinta();
    valoracion_actualiza_texto(true);
}

/* Marcar la casilla de servicios abre la pantalla; desmarcarla borra lo que
 * hubiera puesto, que sin valoracion no significa nada.
 *
 * El estado de un checkbox de LVGL cambia al SOLTAR, no al presionar, asi que
 * abrir otra pantalla desde aqui es seguro: cuando llega este aviso el dedo ya
 * se ha levantado y no queda ningun toque pendiente. */
static void valoracion_toggle_cb(lv_event_t *e)
{
    bool marcado = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
    if (!marcado) valoracion_reset();
    valoracion_actualiza_texto(marcado);
    if (marcado) show_form(CAT_VALORACION);
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

/* Devuelve el rotulo del titulo: la pantalla de viaje lo reescribe segun haya
 * viaje en marcha o no. 'back_to' es a donde lleva el Volver (BACK_TO_GRID al
 * menu de iconos, o el indice de otra pantalla). */
static lv_obj_t *add_header(lv_obj_t *form, const char *title, lv_color_t color,
                             int back_to)
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
    lv_obj_add_event_cb(back, back_click_cb, LV_EVENT_CLICKED,
                        (void *)(intptr_t)(back_to + 1));
    lv_obj_t *back_lbl = lv_label_create(back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT "  Volver");
    lv_obj_set_style_text_font(back_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(back_lbl, color, 0);
    /* Al pulsar el fondo se vuelve del color (claro), asi que el texto pasa a
     * NEGRO, no a blanco: sobre estos tonos vivos el blanco se pierde. */
    lv_obj_set_style_text_color(back_lbl, lv_color_hex(COL_TILE_FG), LV_STATE_PRESSED);
    lv_obj_center(back_lbl);

    /* Centrado en la fila, no respecto al hueco que deja el boton: asi el
     * titulo cae en el eje de la pantalla.
     *
     * Con ANCHO TOPE, y no es un adorno: el boton ocupa de 0 a 128, asi que un
     * titulo centrado solo puede medir 2*(232-128) = 208 px antes de metersele
     * encima. "SERVICIOS DEL AREA" media ~219 y lo pisaba. Fijando el ancho, un
     * titulo demasiado largo sale con puntos suspensivos -- feo, pero visible y
     * sin tapar el boton, en vez de solaparse en silencio.
     * En letra 22 caben ~14 caracteres: "VIAJE EN CURSO" (~175 px) y
     * "MANTENIMIENTO" (~163 px) entran holgados. */
    lv_obj_t *t = lv_label_create(row);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_color(t, color, 0);
    lv_obj_set_style_text_font(t, &lv_font_montserrat_22, 0);
    lv_obj_set_width(t, HEADER_TITLE_MAX_W);
    lv_label_set_long_mode(t, LV_LABEL_LONG_DOT);
    lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_align(t, LV_ALIGN_CENTER, 0, 0);
    return t;
}

/* === Callbacks de guardado (solo log, ver Fase 4) ======================= */

/* === Confirmacion antes de guardar ====================================== */

static const char *CAT_NOMBRE[CAT_COUNT] = {
    "viaje", "repostaje", "peaje", "bombona", "mantenimiento",
    "parada", "servicios", "valoracion"
};

/* CLAVES para el JSON y las columnas del CSV de la P4. Separadas de los rotulos
 * de pantalla A PROPOSITO: los rotulos pueden cambiar de redaccion cuando el
 * usuario pida otra cosa, y si las columnas fueran los rotulos, ese cambio
 * partiria en dos el historico de todos los viajes anteriores.
 *
 * Sin espacios ni acentos: acaban siendo cabeceras de una hoja de calculo.
 * EL ORDEN NO SE TOCA -- es el de las columnas. Añadir al final. */
#define CAT_CLAVE CAT_NOMBRE

static const char *const MANT_CLAVE[MANT_COUNT] = {
    "aceite", "filtro_aceite", "filtro_aire", "filtro_habitaculo",
    "correa", "ruedas"
};
static const char *const PARADA_CLAVE[PARADA_COUNT] = {
    "vaciado", "llenado", "agua_potable", "pernocta_gratis", "area", "camping"
};
/* Solo los seis servicios; el septimo de SERV_OPCIONES es la puerta a la
 * pantalla de valoracion, no un servicio. */
static const char *const SERV_CLAVE[SERV_IDX_VALORACION] = {
    "serv_agua", "serv_vaciado_grises", "serv_vaciado_wc",
    "serv_electricidad", "serv_duchas", "serv_basura"
};
static const char *const VAL_EXTRA_CLAVE[VAL_EXTRA_COUNT] = {
    "ruidoso", "sin_sombra"
};

static uint32_t cat_color(categoria_t c)
{
    switch (c) {
        case CAT_VIAJE:         return COL_VIAJE;
        case CAT_REPOSTAJE:     return COL_REPOSTAJE;
        case CAT_PEAJE:         return COL_PEAJE;
        case CAT_BOMBONA:       return COL_BOMBONA;
        /* Parada y servicios cuelgan de Viaje: van con su azul para que se vea
         * que son la misma rama. */
        case CAT_PARADA:
        case CAT_SERVICIOS:
        case CAT_VALORACION:    return COL_VIAJE;
        default:                return COL_MANTENIMIENTO;
    }
}

/* Codigo de moneda de un dropdown, en corto ("EUR"). */
static const char *currency_of(lv_obj_t *dd)
{
    uint16_t i = lv_dropdown_get_selected(dd);
    return i < (sizeof(CURRENCY_CODES) / sizeof(CURRENCY_CODES[0]))
           ? CURRENCY_CODES[i] : "EUR";
}

/* Un campo vacio se muestra como "--" para que se vea que falta, en vez de
 * dejar la linea a medias. */
static const char *val_or_dash(lv_obj_t *ta)
{
    const char *t = lv_textarea_get_text(ta);
    return (t && t[0]) ? t : "--";
}

/* Buffer del resumen. Estatico porque el dialogo lo sigue leyendo despues de
 * que save_generic_cb() haya terminado. */
static char s_resumen[256];

static void build_resumen(categoria_t cat)
{
    /* 96: con las SEIS casillas marcadas la lista es "Aceite, Filtro aceite,
     * Filtro aire, Filtro habitaculo, Correa, Ruedas x4" -- 72 caracteres.
     * Si se anaden opciones, recalcular. */
    char tipo[96];
    switch (cat) {
        case CAT_REPOSTAJE:
            snprintf(s_resumen, sizeof(s_resumen),
                     "Importe:  %s %s\nLitros:  %s\nPrecio/L:  %s",
                     val_or_dash(s_repo_importe_ta), currency_of(s_repo_currency_dd),
                     val_or_dash(s_repo_litros_ta),
                     lv_label_get_text(s_repo_preciolitro_lbl));
            break;
        case CAT_PEAJE:
            snprintf(s_resumen, sizeof(s_resumen), "Importe:  %s %s",
                     val_or_dash(s_peaje_importe_ta),
                     currency_of(s_peaje_currency_dd));
            break;
        case CAT_BOMBONA:
            snprintf(s_resumen, sizeof(s_resumen),
                     "Bombonas:  %u\nPrecio:  %s %s",
                     (unsigned)(btnmatrix_checked(s_bombona_cuantas_bm, 2) + 1),
                     val_or_dash(s_bombona_precio_ta),
                     currency_of(s_bombona_currency_dd));
            break;
        case CAT_MANTENIMIENTO: {
            /* Lista con lo marcado. Si no hay nada, "--": guardar un
             * mantenimiento sin decir que se hizo no tiene sentido, y asi se
             * ve en la propia confirmacion antes de aceptar. */
            tipo[0] = '\0';
            size_t used = 0;
            for (uint8_t i = 0; i < MANT_COUNT; i++) {
                if (!lv_obj_has_state(s_mant_chk[i], LV_STATE_CHECKED)) continue;
                /* Las ruedas se anotan con su cantidad: "Ruedas x2". */
                int w;
                if (i == MANT_IDX_RUEDAS) {
                    w = snprintf(tipo + used, sizeof(tipo) - used, "%sRuedas x%u",
                                 used ? ", " : "",
                                 ruedas_elegidas());
                } else {
                    w = snprintf(tipo + used, sizeof(tipo) - used, "%s%s",
                                 used ? ", " : "", MANT_OPCIONES[i]);
                }
                if (w < 0 || (size_t)w >= sizeof(tipo) - used) break;
                used += (size_t)w;
            }
            snprintf(s_resumen, sizeof(s_resumen), "%s\nKm:  %s\nCoste:  %s",
                     used ? tipo : "--", val_or_dash(s_mant_km_ta),
                     val_or_dash(s_mant_coste_ta));
            break;
        }
        case CAT_PARADA: {
            /* CUATRO lineas como mucho: el cuerpo va en letra 32 (unos 46 px
             * por linea con el interlineado) y entre el titulo y los botones
             * quedan ~184 px (ver el reparto en confirm_screen.c). De ahi los
             * nombres cortos, aqui y en los servicios.
             *
             * Peor caso con los cortos: "Vaciado, Llenado, Agua, Pernocta" son
             * 32 caracteres, o sea 2 lineas, mas precio (1) y servicios (1). */
            tipo[0] = '\0';
            size_t used = 0;
            for (uint8_t i = 0; i < PARADA_COUNT; i++) {
                if (!lv_obj_has_state(s_parada_chk[i], LV_STATE_CHECKED)) continue;
                int w = snprintf(tipo + used, sizeof(tipo) - used, "%s%s",
                                 used ? ", " : "", PARADA_CORTOS[i]);
                if (w < 0 || (size_t)w >= sizeof(tipo) - used) break;
                used += (size_t)w;
            }

            char extra[160];
            extra[0] = '\0';
            size_t e = 0;
            /* Precio y servicios van juntos: los dos son cosa de las paradas
             * que se pagan (area o camping) y en una pernocta gratis no se
             * pintan ni el uno ni los otros. */
            if (parada_es_de_pago()) {
                /* "Precio/noche" y no "Precio por noche": la linea entera tiene
                 * que caber en ~25 caracteres y "Precio por noche:  25.00 EUR"
                 * son 28. En la pantalla, donde hay sitio, si va entero. */
                int w = snprintf(extra, sizeof(extra), "\nPrecio/%s:  %s %s",
                                 parada_cobro_actual() == PARADA_COBRO_24H
                                 ? "24h" : "noche",
                                 val_or_dash(s_parada_precio_ta),
                                 currency_of(s_parada_currency_dd));
                if (w > 0) e = (size_t)w;

                /* Los servicios marcados, o su cuenta si la lista no cabe en
                 * una linea: mas vale un "4 de 6" exacto que una linea partida
                 * en dos que empuje los botones fuera de la pantalla. */
                char serv[128];
                serv[0] = '\0';
                size_t s_used = 0;
                uint8_t marcados = 0;
                for (uint8_t i = 0; i < SERV_COUNT; i++) {
                    if (!lv_obj_has_state(s_serv_chk[i], LV_STATE_CHECKED)) continue;
                    marcados++;
                    /* En el sitio de "Valoracion" va la nota elegida, que es el
                     * dato; la palabra sola no dice nada. */
                    /* Con su nombre entero, no abreviados: el dialogo baja la
                     * letra si hace falta y "Vaciado grises" se entiende sin
                     * pensar, cosa que "Grises" no. En el sitio de "Valoracion"
                     * va la nota elegida, que es el dato. */
                    const char *nombre = (i == SERV_IDX_VALORACION)
                                         ? valoracion_elegida() : SERV_OPCIONES[i];
                    int n = snprintf(serv + s_used, sizeof(serv) - s_used, "%s%s",
                                     s_used ? ", " : "", nombre);
                    if (n < 0 || (size_t)n >= sizeof(serv) - s_used) break;
                    s_used += (size_t)n;
                }
                /* Las pegas van detras de la nota y solo si hay valoracion:
                 * viven dentro de esa pantalla y sin ella no se han podido
                 * marcar. Aqui es donde se repasan antes de guardar, porque en
                 * la casilla de servicios no caben. */
                if (lv_obj_has_state(s_serv_chk[SERV_IDX_VALORACION], LV_STATE_CHECKED)) {
                    for (uint8_t i = 0; i < VAL_EXTRA_COUNT; i++) {
                        if (!lv_obj_has_state(s_val_extra_chk[i], LV_STATE_CHECKED)) continue;
                        marcados++;
                        int n = snprintf(serv + s_used, sizeof(serv) - s_used, "%s%s",
                                         s_used ? ", " : "", VAL_EXTRAS[i]);
                        if (n < 0 || (size_t)n >= sizeof(serv) - s_used) break;
                        s_used += (size_t)n;
                    }
                }
                /* Se describen SIEMPRE, por largos que sean: el dialogo baja
                 * la letra cuando hace falta (ver confirm_screen.c) y una lista
                 * entera dice mucho mas que un "6 marcados" que obliga a
                 * volver atras para saber cuales. */
                snprintf(extra + e, sizeof(extra) - e, "\nServicios:  %s",
                         marcados ? serv : "--");
            }
            snprintf(s_resumen, sizeof(s_resumen), "%s%s", used ? tipo : "--", extra);
            break;
        }
        default:
            s_resumen[0] = '\0';
            break;
    }
}

/* === Parada abierta: la que dura varios dias ==============================
 *
 * Una parada en un area, un camping o una pernocta no termina cuando la
 * guardas: termina cuando te vas, que puede ser dias despues y con la pantalla
 * apagada por medio (se va con el contacto). Asi que al guardarla queda
 * ABIERTA, y al volver a encender se pregunta si ha terminado.
 *
 * El unico reloj que hay es el dia de calendario que manda la P4 (mini_proto.h):
 * esta pantalla no tiene RTC ni pila. Sin ese dato no se abre parada -- no
 * habria forma de contar las noches y quedaria colgada preguntando algo
 * incontestable. */
static uint32_t reloj_p4(void)
{
    mini_data_t d;
    data_model_get(&d);
    return d.epoch_local;     /* 0 = la P4 aun no ha dicho la hora */
}

/* Cuanto se cobra de una parada, en noches o en periodos de 24 h.
 *
 * NOCHE: lo que se cuenta son cambios de dia de calendario. El reloj viene ya
 * en hora local (mini_proto.h), asi que la division entera da el dia directo.
 * Llegas el viernes por la tarde y te vas el sabado por la manana: una noche.
 *
 * 24 H: periodos desde que entras, REDONDEANDO HACIA ARRIBA -- 25 horas son 2.
 * Es como cobran ellos: pasado el plazo empieza otro, y mas vale que la cuenta
 * salga alta y no baja.
 *
 * Minimo 1 en los dos casos: si llegas y te vas el mismo dia la parada ha
 * existido igual, y se paga igual. */
static unsigned parada_unidades(uint32_t inicio, uint32_t ahora, uint8_t cobro)
{
    if (ahora <= inicio) return 1;

    unsigned n;
    if (cobro == PARADA_COBRO_24H) {
        uint32_t transcurrido = ahora - inicio;
        n = (unsigned)((transcurrido + 86399u) / 86400u);
    } else {
        n = (unsigned)((ahora / 86400u) - (inicio / 86400u));
    }
    return n ? n : 1;
}

/* Cual de los tres sitios esta marcado, o -1 si la parada fue solo de vaciado,
 * llenado o agua: esas se acaban en el sitio y no dejan nada abierto. */
static int parada_lugar_marcado(void)
{
    for (uint8_t i = 0; i < sizeof(PARADA_LUGARES); i++) {
        if (lv_obj_has_state(s_parada_chk[PARADA_LUGARES[i]], LV_STATE_CHECKED)) {
            return PARADA_LUGARES[i];
        }
    }
    return -1;
}

/* Se llama con el formulario TODAVIA lleno: show_grid() lo vacia justo
 * despues. */
static void parada_abrir_si_procede(void)
{
    int lugar = parada_lugar_marcado();
    if (lugar < 0) return;

    uint32_t ahora = reloj_p4();
    if (ahora == 0) {
        /* Callarse aqui era un fallo: guardabas la parada, la pantalla decia
         * "guardado" y por dentro no apuntaba nada, asi que te ibas creyendo
         * que se estaba contando la estancia. */
        ESP_LOGW(TAG, "Parada guardada SIN abrir: la P4 no ha dado la hora "
                      "todavia, no se podria contar el tiempo");
        confirm_screen_aviso("Parada sin contar",
                             "Sin la P4 no se que dia es,\nasi que no puedo contar\n"
                             "los dias de esta parada.",
                             COL_ACCION_STOP, "Entendido");
        return;
    }

    parada_abierta_t p = {
        .abierta      = true,
        .lugar        = (uint8_t)lugar,
        .epoch_inicio = ahora,
        .cobro        = parada_cobro_actual(),
        .moneda       = (uint8_t)lv_dropdown_get_selected(s_parada_currency_dd),
    };
    snprintf(p.precio, sizeof(p.precio), "%s",
             lv_textarea_get_text(s_parada_precio_ta));

    esp_err_t err = save_parada_abierta(&p);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo guardar la parada abierta: %s", esp_err_to_name(err));
        return;
    }
    s_parada_abierta = true;
    viaje_refresh();
    ESP_LOGI(TAG, "Parada ABIERTA en '%s' (cobro %s) en t=%lu",
             PARADA_OPCIONES[lugar],
             p.cobro == PARADA_COBRO_24H ? "24h" : "noche",
             (unsigned long)ahora);
}

/* Monta el apunte de la categoria y lo mete en la cola.
 *
 * El resumen que va al diario del viaje se reaprovecha de s_resumen, el mismo
 * que acabas de ver en la confirmacion: si lo que se guarda no coincidiera con
 * lo que te enseño la pantalla, seria un fallo dificil de pillar. Se le quitan
 * los saltos de linea, que ahi eran para leerlo y en un CSV sobran. */
static void apunte_encolar(categoria_t cat)
{
    /* 640 y no 384: la parada completa son 505 bytes y con 384 se cortaba a
     * medias (ver la cabecera de apunte.c). Queda margen para los dos campos de
     * posicion que traera el GPS de la P4. */
    char b[640];
    size_t u = apunte_cabecera(b, sizeof(b), next_trip_seq(), CAT_CLAVE[cat]);

    switch (cat) {
        case CAT_REPOSTAJE:
            u = apunte_campo_txt(b, sizeof(b), u, "moneda", currency_of(s_repo_currency_dd));
            u = apunte_campo_txt(b, sizeof(b), u, "importe", lv_textarea_get_text(s_repo_importe_ta));
            u = apunte_campo_txt(b, sizeof(b), u, "litros",  lv_textarea_get_text(s_repo_litros_ta));
            u = apunte_campo_txt(b, sizeof(b), u, "precio_litro", lv_label_get_text(s_repo_preciolitro_lbl));
            break;
        case CAT_PEAJE:
            u = apunte_campo_txt(b, sizeof(b), u, "moneda", currency_of(s_peaje_currency_dd));
            u = apunte_campo_txt(b, sizeof(b), u, "importe", lv_textarea_get_text(s_peaje_importe_ta));
            break;
        case CAT_BOMBONA:
            u = apunte_campo_num(b, sizeof(b), u, "cuantas",
                                 btnmatrix_checked(s_bombona_cuantas_bm, 2) + 1);
            u = apunte_campo_txt(b, sizeof(b), u, "moneda", currency_of(s_bombona_currency_dd));
            u = apunte_campo_txt(b, sizeof(b), u, "precio", lv_textarea_get_text(s_bombona_precio_ta));
            break;
        case CAT_MANTENIMIENTO:
            /* Una columna por casilla, con 0/1. Asi se pueden sumar y filtrar
             * en la hoja de calculo; una lista de texto no se puede. */
            for (uint8_t i = 0; i < MANT_COUNT; i++) {
                u = apunte_campo_num(b, sizeof(b), u, MANT_CLAVE[i],
                                     lv_obj_has_state(s_mant_chk[i], LV_STATE_CHECKED) ? 1 : 0);
            }
            u = apunte_campo_num(b, sizeof(b), u, "ruedas_n",
                                 lv_obj_has_state(s_mant_chk[MANT_IDX_RUEDAS], LV_STATE_CHECKED)
                                 ? (long)ruedas_elegidas() : 0);
            u = apunte_campo_txt(b, sizeof(b), u, "km",    lv_textarea_get_text(s_mant_km_ta));
            u = apunte_campo_txt(b, sizeof(b), u, "coste", lv_textarea_get_text(s_mant_coste_ta));
            break;
        case CAT_PARADA: {
            for (uint8_t i = 0; i < PARADA_COUNT; i++) {
                u = apunte_campo_num(b, sizeof(b), u, PARADA_CLAVE[i],
                                     lv_obj_has_state(s_parada_chk[i], LV_STATE_CHECKED) ? 1 : 0);
            }
            u = apunte_campo_txt(b, sizeof(b), u, "cobro",
                                 parada_cobro_actual() == PARADA_COBRO_24H ? "24h" : "noche");
            u = apunte_campo_txt(b, sizeof(b), u, "moneda", currency_of(s_parada_currency_dd));
            u = apunte_campo_txt(b, sizeof(b), u, "precio", lv_textarea_get_text(s_parada_precio_ta));
            for (uint8_t i = 0; i < SERV_IDX_VALORACION; i++) {
                u = apunte_campo_num(b, sizeof(b), u, SERV_CLAVE[i],
                                     lv_obj_has_state(s_serv_chk[i], LV_STATE_CHECKED) ? 1 : 0);
            }
            u = apunte_campo_txt(b, sizeof(b), u, "valoracion", VALORACION[s_val_nota]);
            for (uint8_t i = 0; i < VAL_EXTRA_COUNT; i++) {
                u = apunte_campo_num(b, sizeof(b), u, VAL_EXTRA_CLAVE[i],
                                     lv_obj_has_state(s_val_extra_chk[i], LV_STATE_CHECKED) ? 1 : 0);
            }
            /* Como quedo aparcada. Es un dato DEL SITIO, no del momento: si un
             * area tiene mucha pendiente, conviene saberlo antes de volver. */
            float pitch = 0, roll = 0;
            if (tilt_get(&pitch, &roll)) {
                u = apunte_campo_num(b, sizeof(b), u, "cabeceo_centi", (long)(pitch * 100));
                u = apunte_campo_num(b, sizeof(b), u, "balanceo_centi", (long)(roll * 100));
            }
            break;
        }
        default:
            break;
    }

    /* El resumen, en una linea. */
    char resumen[96];
    size_t j = 0;
    for (size_t i = 0; s_resumen[i] && j + 1 < sizeof(resumen); i++) {
        resumen[j++] = (s_resumen[i] == '\n') ? ' ' : s_resumen[i];
    }
    resumen[j] = 0;
    u = apunte_cerrar(b, sizeof(b), u, resumen);

    /* Se cuenta ANTES de encolar y pase lo que pase: si el apunte se pierde,
     * la P4 tiene que enterarse de que le falta uno. */
    trip_eventos_inc();

    if (u == 0) {
        /* No cabe. No deberia pasar nunca -- el buffer se dimensiono para el
         * apunte mas grande con margen -- pero si alguien añade campos y se
         * pasa, mas vale un cartel que un dato perdido en silencio. */
        ESP_LOGE(TAG, "el apunte '%s' NO CABE en el buffer, no se manda", CAT_NOMBRE[cat]);
        confirm_screen_aviso("No he podido apuntarlo",
                             "Este apunte es demasiado largo\ny no se ha guardado. Avisa de\nesto, es un fallo del programa.",
                             COL_ACCION_STOP, "Entendido");
        return;
    }

    if (!viaje_cola_push(b)) {
        confirm_screen_aviso("No he podido apuntarlo",
                             "La cola de pendientes esta\nllena. Enciende la P4 para\nque se vacie.",
                             COL_ACCION_STOP, "Entendido");
    }
}

/* La accion de verdad, ya confirmada. */
static void do_save(void *user_data)
{
    categoria_t cat = (categoria_t)(uintptr_t)user_data;

    /* Sin viaje en marcha no hay carpeta donde escribirlo, asi que se queda
     * solo en la pantalla. Se avisa en vez de callarse: guardar un repostaje
     * que no va a ninguna parte y no decirlo es justo lo que hace que te fies
     * de un dato que no existe. */
    if (!s_viaje_activo) {
        ESP_LOGW(TAG, "'%s' NO se manda: no hay viaje en marcha", CAT_NOMBRE[cat]);
        if (cat == CAT_PARADA) parada_abrir_si_procede();
        confirm_screen_aviso("Guardado solo aqui",
                             "No hay ningun viaje en marcha,\nasi que esto no se guarda en\nla P4. Inicia un viaje antes.",
                             COL_ACCION_STOP, "Entendido");
        show_grid();
        return;
    }

    apunte_encolar(cat);
    if (cat == CAT_PARADA) parada_abrir_si_procede();
    show_grid();
}

/* --- Fin de parada, al volver a encender ---------------------------------- */

static char s_fin_resumen[96];

static void parada_do_cerrar(void *ud)
{
    (void)ud;
    ESP_LOGI(TAG, "CONFIRMADO fin de parada -- TODO Fase 4: enviar a la P4");
    clear_parada_abierta();
    s_parada_abierta = false;
    viaje_refresh();
}

/* Arma y saca el cartel de fin de parada. Devuelve false si ahora mismo no se
 * puede: o no hay parada abierta, o la P4 todavia no ha dicho la hora y sin
 * ella no hay nada que calcular. */
static bool parada_pregunta_fin(void)
{
    parada_abierta_t p;
    load_parada_abierta(&p);
    if (!p.abierta) return false;

    uint32_t ahora = reloj_p4();
    if (ahora == 0) return false;

    unsigned n = parada_unidades(p.epoch_inicio, ahora, p.cobro);

    const char *sitio = (p.lugar < PARADA_COUNT) ? PARADA_OPCIONES[p.lugar] : "Parada";
    const char *moneda = (p.moneda < (sizeof(CURRENCY_CODES) / sizeof(CURRENCY_CODES[0])))
                         ? CURRENCY_CODES[p.moneda] : "EUR";

    /* "3 noches" o "3 x 24 h", segun como cobre el sitio. */
    char cuanto[24];
    if (p.cobro == PARADA_COBRO_24H) {
        snprintf(cuanto, sizeof(cuanto), "%u x 24 h", n);
    } else {
        snprintf(cuanto, sizeof(cuanto), "%u noche%s", n, n == 1 ? "" : "s");
    }

    /* El total solo si hay precio: una pernocta gratis no lo lleva. */
    if (p.precio[0]) {
        snprintf(s_fin_resumen, sizeof(s_fin_resumen), "%s\n%s\nTotal:  %.2f %s",
                 sitio, cuanto, atof(p.precio) * (double)n, moneda);
    } else {
        snprintf(s_fin_resumen, sizeof(s_fin_resumen), "%s\n%s", sitio, cuanto);
    }

    /* El "no" aqui no es corregir nada: es que sigues en el sitio otro dia
     * mas. La parada se queda abierta y se volvera a preguntar. */
    confirm_screen_open("Fin de la parada?", s_fin_resumen, COL_VIAJE,
                        "Si, terminar", "No, continuar", parada_do_cerrar, NULL);
    return true;
}

/* Al arrancar: espera a que la P4 diga la hora y pregunta, una sola vez.
 * Contestar que NO deja la parada abierta y no se vuelve a preguntar sola --
 * para eso esta el boton de la pantalla de Viaje, que la cierra cuando tu
 * quieras.
 *
 * Si la P4 no aparece (apagada o fuera de alcance) no se pregunta nada y el
 * temporizador sigue mirando: mas vale callar que inventarse las noches. */
static void parada_fin_timer_cb(lv_timer_t *t)
{
    parada_abierta_t p;
    load_parada_abierta(&p);
    if (!p.abierta) {          /* se cerro, o nunca la hubo */
        lv_timer_del(t);
        return;
    }
    if (parada_pregunta_fin()) lv_timer_del(t);
}

/* El boton de la pantalla de Viaje. A diferencia del aviso del arranque, aqui
 * lo has pedido tu: si no se puede, hay que decir por que en vez de no hacer
 * nada, que parece que el boton esta roto. */
static void parada_fin_click_cb(lv_event_t *e)
{
    (void)e;
    if (parada_pregunta_fin()) return;
    confirm_screen_aviso("Sin la P4",
                         "No se que dia es, asi que\nno puedo calcular lo que\n"
                         "ha durado la parada.",
                         COL_ACCION_STOP, "Entendido");
}

static void save_generic_cb(lv_event_t *e)
{
    categoria_t cat = (categoria_t)(uintptr_t)lv_event_get_user_data(e);
    build_resumen(cat);
    confirm_screen_open("Es correcto?", s_resumen, cat_color(cat), "Si, guardar",
                        NULL, do_save, (void *)(uintptr_t)cat);
}

/* Pone al dia las dos caras de la pantalla de viaje y el texto de su casilla
 * en el menu. Sin viaje: mensaje + "Iniciar viaje" grande, y NADA de
 * finalizar -- no se puede terminar lo que no ha empezado. Con viaje:
 * "Anotar parada" grande y "Finalizar viaje" pequeno abajo, lejos del pulgar
 * que viene de anotar. */
static void viaje_refresh(void)
{
    lv_label_set_text(s_viaje_title_lbl, s_viaje_activo ? "VIAJE EN CURSO" : "VIAJE");
    set_hidden(s_viaje_msg,           s_viaje_activo);
    set_hidden(s_viaje_btn_iniciar,   s_viaje_activo);
    set_hidden(s_viaje_btn_parada,   !s_viaje_activo);
    set_hidden(s_viaje_btn_finalizar, !s_viaje_activo);
    /* La parada se puede cerrar SIEMPRE que este abierta, haya viaje o no: si
     * el viaje se termino con una parada sin cerrar, sigue habiendo que
     * cerrarla y este es el unico sitio desde donde hacerlo. */
    set_hidden(s_viaje_btn_fin_parada, !s_parada_abierta);
}

static void viaje_set_activo(bool activo)
{
    s_viaje_activo = activo;
    esp_err_t err = save_trip_active(activo);
    if (err != ESP_OK) {
        /* Se sigue adelante: el viaje vale para esta sesion, solo se pierde si
         * se va la luz. Peor seria no dejar iniciarlo por un fallo de NVS. */
        ESP_LOGW(TAG, "No se pudo guardar el estado del viaje: %s", esp_err_to_name(err));
    }
    viaje_refresh();
}

/* --- Envio a la P4 (Fase 4, fase 1) ---------------------------------------
 *
 * El viaje NO se da por iniciado hasta que la P4 confirma. Es lo contrario de
 * lo que hacia antes (marcarlo aqui y ya): si se marcara sin confirmacion, la
 * 3.5" diria "viaje en curso" mientras en la tarjeta de la P4 no hay carpeta
 * ninguna, y todo lo que se anotara despues iria a un viaje que no existe. */

static void aviso_envio_fallo(int estado, const char *que)
{
    char cuerpo[160];
    if (estado == 401) {
        snprintf(cuerpo, sizeof(cuerpo),
                 "La P4 no acepta la clave.\nRevisa usuario y clave en\nAjustes.");
    } else if (estado == 409) {
        snprintf(cuerpo, sizeof(cuerpo),
                 "La P4 dice que ya hay un\nviaje abierto. Terminalo\nantes de empezar otro.");
    } else if (estado == 0) {
        snprintf(cuerpo, sizeof(cuerpo),
                 "No he podido hablar con la P4.\nComprueba que este encendida.");
    } else {
        snprintf(cuerpo, sizeof(cuerpo), "La P4 ha respondido %d\ny no se ha guardado.", estado);
    }
    char titulo[40];
    snprintf(titulo, sizeof(titulo), "%s sin guardar", que);
    confirm_screen_aviso(titulo, cuerpo, COL_ACCION_STOP, "Entendido");
}

static void inicio_resultado_cb(bool ok, int estado)
{
    if (!ok) { aviso_envio_fallo(estado, "Viaje"); return; }
    save_trip_destino(s_viaje_destino);
    trip_eventos_reset();
    viaje_set_activo(true);
    /* La P4 ya ha creado su carpeta; aqui se abre la salida que sostiene los
     * menus. Si fallase (sin hora no puede ser: la acabamos de usar) el viaje
     * quedaria en la P4 y no en la pantalla, y se veria al momento. */
    salida_abrir_viaje(s_viaje_destino);
    ESP_LOGI(TAG, "viaje iniciado en la P4, destino '%s'", s_viaje_destino);
    show_grid();
}

/* Viaje no lleva resumen: no hay nada tecleado que repasar. Solo el segundo
 * toque, que aqui importa mas que en ningun sitio -- finalizar un viaje por un
 * roce cierra el registro en curso de la P4. */
static void viaje_do_iniciar(void *ud)
{
    (void)ud;
    uint32_t ahora = reloj_p4();
    if (ahora == 0) {   /* comprobado tambien antes de teclear; puede caerse en medio */
        confirm_screen_aviso("Enciende la P4 primero",
                             "Sin ella no se que dia es,\ny la carpeta del viaje lleva\nla fecha en el nombre.",
                             COL_ACCION_STOP, "Entendido");
        return;
    }
    if (!p4_api_viaje_inicio(next_trip_seq(), s_viaje_destino,
                             ahora / 86400u, inicio_resultado_cb)) {
        aviso_envio_fallo(0, "Viaje");
    }
}

/* El FIN va por la COLA, no directo como el inicio, y la diferencia es a
 * proposito:
 *
 *  - El inicio EXIGE la P4 delante (la carpeta lleva la fecha en el nombre), y
 *    hasta que no confirma no hay viaje. Encolarlo no tendria sentido: no se
 *    puede anotar nada de un viaje cuya carpeta aun no existe.
 *  - El fin se apunta cuando llegas, que es justo cuando puedes haber apagado
 *    ya la P4. Y ademas entra en la cola DETRAS de los registros pendientes,
 *    asi que nunca los adelanta: la P4 no cerrara el viaje con apuntes suyos
 *    todavia por llegar.
 *
 * Por eso aqui el viaje se da por terminado en el acto aunque no se haya
 * entregado: la cola sobrevive al apagon y lo entregara. Lo que no puede pasar
 * es que te quedes con "viaje en curso" en pantalla porque la P4 estaba
 * apagada. */
static void viaje_do_finalizar(void *ud)
{
    (void)ud;
    /* trip_eventos_GET y no _inc: el fin es el ultimo evento, no hay nada
     * despues que contar, y con _inc cada reintento fallido subia el contador.
     * Tres intentos con la cola llena y la P4 daba el viaje por incompleto sin
     * faltarle nada. El +1 es este mismo mensaje. */
    char cuerpo[80];
    p4_api_cuerpo_fin(cuerpo, sizeof(cuerpo), next_trip_seq(), trip_eventos_get() + 1);

    if (!viaje_cola_push(cuerpo)) {
        confirm_screen_aviso("No he podido apuntarlo",
                             "La cola de pendientes esta\nllena. Enciende la P4 para\nque se vacie.",
                             COL_ACCION_STOP, "Entendido");
        return;
    }
    save_trip_destino("");
    viaje_set_activo(false);
    salida_cerrar();
    show_grid();
}

/* "Anotar parada" y "Servicios del area" son navegacion, no guardado: van
 * directas a su pantalla, sin confirmacion. */
static void parada_open_cb(lv_event_t *e)
{
    (void)e;
    show_form(CAT_PARADA);
}

static void servicios_open_cb(lv_event_t *e)
{
    (void)e;
    /* El rotulo dice que se esta marcando, que no es lo mismo segun donde
     * hayas parado: en un area, lo que la instalacion ofrece; en un camping,
     * lo que ya va pagado en el precio de la noche. */
    bool camping = lv_obj_has_state(s_parada_chk[PARADA_IDX_CAMPING], LV_STATE_CHECKED);
    lv_label_set_text(s_serv_hint, camping ? "Incluido en el precio"
                                           : "Lo que ofrece el area");
    show_form(CAT_SERVICIOS);
}

/* Al marcar un sitio se desmarcan los otros dos, en vez de bloquearlos en gris:
 * asi cambiar de idea es UN toque sobre el que quieres, sin acordarse de quitar
 * antes el anterior -- que es lo que hace falta con el vehiculo parando.
 *
 * No se llama a si mismo en cadena: lv_obj_clear_state() no dispara
 * LV_EVENT_VALUE_CHANGED, solo lo hace el toque del usuario. */
static void parada_lugar_cb(lv_event_t *e)
{
    lv_obj_t *target = lv_event_get_target(e);
    if (lv_obj_has_state(target, LV_STATE_CHECKED)) {
        for (uint8_t i = 0; i < sizeof(PARADA_LUGARES); i++) {
            lv_obj_t *otro = s_parada_chk[PARADA_LUGARES[i]];
            if (otro != target) lv_obj_clear_state(otro, LV_STATE_CHECKED);
        }
    }
    /* Cambiar de sitio borra precio y servicios: eran del sitio anterior. */
    parada_clear_extras();
    parada_refresh_extras();
}

/* Se llama al aceptar el editor del destino. */
static void viaje_destino_listo_cb(lv_event_t *e)
{
    (void)e;
    const char *txt = lv_textarea_get_text(s_viaje_destino_ta);
    if (!txt || !txt[0]) return;      /* lo dejo en blanco: no se hace nada */
    snprintf(s_viaje_destino, sizeof(s_viaje_destino), "%s", txt);

    char cuerpo[120];
    snprintf(cuerpo, sizeof(cuerpo), "Destino: %s\n\nLa carpeta se llamara asi\ny NO se podra cambiar.",
             s_viaje_destino);
    confirm_screen_open("Empezar el viaje?", cuerpo, COL_ACCION_OK,
                        "Si, empezar", "Cancelar", viaje_do_iniciar, NULL);
}

static void viaje_iniciar_cb(lv_event_t *e)
{
    (void)e;
    /* Se exige la P4 ANTES de teclear nada: la carpeta del viaje lleva la fecha
     * en el nombre y este aparato no tiene reloj propio. Preguntar el destino
     * para luego no poder empezar seria hacer teclear en balde. */
    if (reloj_p4() == 0) {
        confirm_screen_aviso("Enciende la P4 primero",
                             "Sin ella no se que dia es,\ny la carpeta del viaje lleva\nla fecha en el nombre.",
                             COL_ACCION_STOP, "Entendido");
        return;
    }
    lv_textarea_set_text(s_viaje_destino_ta, "");
    entry_screen_open(s_viaje_destino_ta, "Destino", false);
}

static void viaje_finalizar_cb(lv_event_t *e)
{
    (void)e;
    confirm_screen_open("Finalizar el viaje?", NULL, COL_ACCION_STOP,
                        "Si, finalizar", "Cancelar", viaje_do_finalizar, NULL);
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
/* 'grande' reparte el hueco sobrante del formulario; sin el, el boton se queda
 * en una pastilla baja al final. "Finalizar viaje" va asi a peticion del
 * usuario: pequeno y abajo, para que la accion habitual (anotar parada) se
 * lleve la pantalla y la destructiva no se toque de un roce. */
#define VIAJE_BTN_PEQ_H  46

static lv_obj_t *make_viaje_button(lv_obj_t *form, const char *text, uint32_t color,
                                    lv_event_cb_t cb, bool grande)
{
    lv_obj_t *btn = lv_btn_create(form);
    lv_obj_set_width(btn, lv_pct(100));
    if (grande) {
        lv_obj_set_height(btn, LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(btn, 70, 0);
        lv_obj_set_flex_grow(btn, 1);
    } else {
        lv_obj_set_height(btn, VIAJE_BTN_PEQ_H);
    }
    lv_obj_set_style_radius(btn, 12, 0);
    lv_obj_set_style_bg_color(btn, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(btn, lv_color_darken(lv_color_hex(color), LV_OPA_30),
                              LV_STATE_PRESSED);
    lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, NULL);

    lv_obj_t *lbl = lv_label_create(btn);
    lv_label_set_text(lbl, text);
    lv_obj_set_style_text_font(lbl, grande ? &lv_font_montserrat_24
                                           : &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TILE_FG), 0);
    lv_obj_center(lbl);
    return btn;
}

static void build_viaje(lv_obj_t *form)
{
    s_viaje_title_lbl = add_header(form, "VIAJE", lv_color_hex(COL_VIAJE),
                                   BACK_TO_GRID);

    /* Sin campo de coordenada GPS manual aqui a proposito: cuando exista
     * el GPS real (Fase 4 + modulo GPS de la P4) la posicion de
     * inicio/fin se capturaria sola en el instante del toque, no tiene
     * sentido pedirla a mano para una accion pensada como "un solo toque". */
    /* Campo INVISIBLE para el destino: el editor a pantalla completa vuelca
     * sobre un textarea, y aqui no hay formulario donde ponerlo. Fuera del
     * layout y de tamano 0 para que no ocupe ni un pixel. */
    s_viaje_destino_ta = lv_textarea_create(form);
    lv_obj_add_flag(s_viaje_destino_ta, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_viaje_destino_ta, LV_OBJ_FLAG_IGNORE_LAYOUT);
    lv_obj_set_size(s_viaje_destino_ta, 0, 0);
    lv_textarea_set_one_line(s_viaje_destino_ta, true);
    /* 20 caracteres: es el limite del diseño, y la ruta en la SD de la P4 no
     * debe crecer sin control. */
    lv_textarea_set_max_length(s_viaje_destino_ta, 20);
    /* SOLO ASCII, y no es un descuido: ver el bloque de monedas arriba -- las
     * fuentes Montserrat compiladas no traen acentos ni la ñ, y saldrian
     * cuadrados. Ademas esto acaba siendo un NOMBRE DE CARPETA, asi que los
     * caracteres que romperian la ruta no se dejan ni teclear. La P4 vuelve a
     * filtrar por su cuenta: no se fia de lo que le manden. */
    lv_textarea_set_accepted_chars(s_viaje_destino_ta,
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789 -_");
    lv_obj_add_event_cb(s_viaje_destino_ta, viaje_destino_listo_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);

    s_viaje_msg = lv_label_create(form);
    lv_label_set_text(s_viaje_msg, "Inicio y fin de viaje de la P4, desde aqui.");
    lv_obj_set_style_text_color(s_viaje_msg, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(s_viaje_msg, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(s_viaje_msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_viaje_msg, lv_pct(100));

    /* Los tres botones existen desde el arranque y se ocultan segun haya viaje
     * o no (viaje_refresh): crearlos y destruirlos a cada entrada seria mas
     * codigo y mas ocasiones de dejarse un puntero colgando. Solo se ve uno
     * grande cada vez, asi que flex_grow le da toda la pantalla. */
    s_viaje_btn_iniciar = make_viaje_button(form, LV_SYMBOL_PLAY "  Iniciar viaje",
                                            COL_ACCION_OK, viaje_iniciar_cb, true);
    s_viaje_btn_parada  = make_viaje_button(form, LV_SYMBOL_PLUS "  Anotar parada",
                                            COL_VIAJE, parada_open_cb, true);
    /* En azul y encima del rojo: cerrar una parada es rutina y terminar el viaje
     * no, asi que se distinguen por color y el destructivo queda el ultimo. */
    s_viaje_btn_fin_parada = make_viaje_button(form, "Finalizar parada",
                                               COL_VIAJE, parada_fin_click_cb, false);
    s_viaje_btn_finalizar = make_viaje_button(form, LV_SYMBOL_STOP "  Finalizar viaje",
                                              COL_ACCION_STOP, viaje_finalizar_cb, false);
}

static void build_repostaje(lv_obj_t *form)
{
    add_header(form, "REPOSTAJE", lv_color_hex(COL_REPOSTAJE), BACK_TO_GRID);

    /* Sin coordenada GPS ni hora a peticion del usuario (20-ago-2026): tecleadas
     * a mano no aportan nada y estorban en el surtidor. Cuando se abra la Fase 4
     * las pone la P4 al recibir el evento, que ya sabe donde y cuando esta. */
    /* Moneda arriba a lo ancho, y debajo importe y litros compartiendo linea:
     * asi los dos numeros caben en letra 32 en vez de 24. */
    s_repo_currency_dd = make_currency_row(form, "Moneda");
    make_dual_number_row(form, "Importe", &s_repo_importe_ta,
                               "Litros",  &s_repo_litros_ta);
    lv_obj_add_event_cb(s_repo_importe_ta, repo_recalc_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_repo_litros_ta, repo_recalc_cb, LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_repo_currency_dd, repo_recalc_cb, LV_EVENT_VALUE_CHANGED, NULL);

    s_repo_preciolitro_lbl = make_readonly_row(form, "Precio/litro (calculado)");

    make_save_button(form, "Guardar repostaje", save_generic_cb, (void *)(uintptr_t)CAT_REPOSTAJE);
}

static void build_peaje(lv_obj_t *form)
{
    add_header(form, "PEAJE", lv_color_hex(COL_PEAJE), BACK_TO_GRID);

    /* Sin coordenada GPS ni hora, igual que repostaje: ver comentario alli.
     * Al ser el unico campo, va en la variante apilada y grande. */
    s_peaje_importe_ta = make_money_field_stacked(form, "Importe",
                                                  &s_peaje_currency_dd);

    make_save_button(form, "Guardar peaje", save_generic_cb, (void *)(uintptr_t)CAT_PEAJE);
}

static void build_bombona(lv_obj_t *form)
{
    add_header(form, "BOMBONA", lv_color_hex(COL_BOMBONA), BACK_TO_GRID);

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

    make_save_button(form, "Guardar bombona", save_generic_cb, (void *)(uintptr_t)CAT_BOMBONA);
}

static void build_mantenimiento(lv_obj_t *form)
{
    add_header(form, "MANTENIMIENTO", lv_color_hex(COL_MANTENIMIENTO), BACK_TO_GRID);

    /* Sin coordenada GPS, igual que repostaje, peaje y bombona: la posicion y
     * la fecha las pone la P4 al recibir el evento (Fase 4). Aqui solo va lo
     * que el aparato no puede deducir. */
    make_check_grid(form, MANT_OPCIONES, MANT_COUNT, s_mant_chk, COL_MANTENIMIENTO,
                    CHK_GAP);

    /* Cuantas ruedas. Oculto salvo que se marque Ruedas: la mayoria de los
     * mantenimientos no las tocan y no tiene sentido ocupar sitio siempre. */
    static const char *ruedas_map[] = { "1", "2", "3", "4", "" };
    s_mant_ruedas_bm = make_choice_row(form, "Cuantas ruedas", ruedas_map);
    lv_obj_add_flag(lv_obj_get_parent(s_mant_ruedas_bm), LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_event_cb(s_mant_chk[MANT_IDX_RUEDAS], ruedas_toggle_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_mant_ruedas_bm, ruedas_num_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);
    lv_obj_add_event_cb(s_mant_ruedas_bm, ruedas_release_cb,
                        LV_EVENT_RELEASED, NULL);

    /* Km y coste comparten linea: los dos son numeros cortos y asi caben los
     * dos grandes sin robarle altura a las seis casillas. */
    make_dual_number_row(form, "Km", &s_mant_km_ta, "Coste", &s_mant_coste_ta);

    make_save_button(form, "Guardar mantenimiento", save_generic_cb, (void *)(uintptr_t)CAT_MANTENIMIENTO);
}

/* Fila de precio de la parada: importe, moneda y tipo de cobro TODOS en la
 * misma linea, y grandes -- son los tres el mismo dato ("cuanto cuesta cada
 * noche / cada 24 h") y leerlos de un vistazo importa mas que su tamano por
 * separado.
 *
 * Reparto elastico y no en pixeles: cuando el sitio es un camping el selector
 * se esconde (alli siempre se cobra por noches) y los otros dos se reparten su
 * hueco solos, en vez de dejar un agujero.
 *
 * No usa make_money_field porque aquella pone el importe y la moneda en letra
 * 24 sin sitio para nada mas. Alturas: rotulo 16 + fila 52 + huecos = 74, que
 * con cabecera 48 + casillas 116 + acciones 50 + 12 de huecos suman 300 de los
 * 304 utiles. */
#define PRECIO_ROW_H   52
#define PRECIO_GROW_TA  4
#define PRECIO_GROW_DD  2
#define PRECIO_GROW_BM  3

static lv_obj_t *make_parada_precio_row(lv_obj_t *parent)
{
    lv_obj_t *cont = make_field_row(parent);

    s_parada_precio_lbl = lv_label_create(cont);
    lv_label_set_text(s_parada_precio_lbl, "Precio");
    lv_obj_set_style_text_color(s_parada_precio_lbl, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(s_parada_precio_lbl, &lv_font_montserrat_16, 0);

    lv_obj_t *row = lv_obj_create(cont);
    lv_obj_set_size(row, lv_pct(100), PRECIO_ROW_H);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_set_style_pad_column(row, 8, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    s_parada_precio_ta = lv_textarea_create(row);
    lv_textarea_set_one_line(s_parada_precio_ta, true);
    lv_textarea_set_placeholder_text(s_parada_precio_ta, "0.00");
    lv_obj_set_height(s_parada_precio_ta, lv_pct(100));
    lv_obj_set_flex_grow(s_parada_precio_ta, PRECIO_GROW_TA);
    lv_obj_set_style_text_font(s_parada_precio_ta, &lv_font_montserrat_32, 0);
    lv_obj_set_style_text_align(s_parada_precio_ta, LV_TEXT_ALIGN_CENTER, 0);
    lv_textarea_set_accepted_chars(s_parada_precio_ta, "0123456789.");
    lv_obj_set_user_data(s_parada_precio_ta, (void *)"Precio por noche");
    lv_obj_add_event_cb(s_parada_precio_ta, ta_click_cb, LV_EVENT_CLICKED,
                        (void *)(uintptr_t)true);

    s_parada_currency_dd = lv_dropdown_create(row);
    lv_dropdown_set_options(s_parada_currency_dd, CURRENCY_OPTIONS);
    lv_obj_set_height(s_parada_currency_dd, lv_pct(100));
    lv_obj_set_flex_grow(s_parada_currency_dd, PRECIO_GROW_DD);
    lv_obj_set_style_text_font(s_parada_currency_dd, &lv_font_montserrat_20, 0);

    /* Excluyente y con "Noche" de partida: es lo normal, y el area de 24 h se
     * marca cuando toca. */
    static const char *cobro_map[] = { "Noche", "24 h", "" };
    s_parada_cobro_bm = lv_btnmatrix_create(row);
    lv_btnmatrix_set_map(s_parada_cobro_bm, cobro_map);
    lv_obj_set_height(s_parada_cobro_bm, lv_pct(100));
    lv_obj_set_flex_grow(s_parada_cobro_bm, PRECIO_GROW_BM);
    lv_obj_set_style_text_font(s_parada_cobro_bm, &lv_font_montserrat_20, 0);
    lv_obj_set_style_bg_opa(s_parada_cobro_bm, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_parada_cobro_bm, 0, 0);
    lv_obj_set_style_pad_all(s_parada_cobro_bm, 0, 0);
    lv_btnmatrix_set_btn_ctrl_all(s_parada_cobro_bm, LV_BTNMATRIX_CTRL_CHECKABLE);
    lv_btnmatrix_set_one_checked(s_parada_cobro_bm, true);
    lv_btnmatrix_set_btn_ctrl(s_parada_cobro_bm, PARADA_COBRO_NOCHE,
                              LV_BTNMATRIX_CTRL_CHECKED);
    /* En RELEASED y no en VALUE_CHANGED: lv_btnmatrix avisa ya al presionar y
     * la marca no se aplica hasta soltar (ver el comentario de las ruedas). */
    lv_obj_add_event_cb(s_parada_cobro_bm, parada_cobro_cb, LV_EVENT_RELEASED, NULL);

    return cont;
}

/* Pantalla de parada. Se llega desde Viaje y su Volver regresa alli, no al
 * menu: es la vuelta natural de donde has venido.
 *
 * Reparto de los 320 px, que van justos: cabecera 48 + cinco casillas en tres
 * filas 116 + precio ~62 + la fila de acciones 54 = 280, mas 12 de huecos,
 * dentro de los 304 utiles. Por eso los servicios del area viven en OTRA
 * pantalla: seis casillas mas (otros 116) no caben de ninguna manera.
 *
 * "Servicios del area" comparte fila con Guardar en vez de llevar la suya: asi
 * no cuesta ni un pixel de alto, y cuando no hay area marcada Guardar se queda
 * con toda la fila. */
static void build_parada(lv_obj_t *form)
{
    add_header(form, "PARADA", lv_color_hex(COL_VIAJE), CAT_VIAJE);

    make_check_grid(form, PARADA_OPCIONES, PARADA_COUNT, s_parada_chk, COL_VIAJE,
                    CHK_GAP);
    for (uint8_t i = 0; i < sizeof(PARADA_LUGARES); i++) {
        lv_obj_add_event_cb(s_parada_chk[PARADA_LUGARES[i]], parada_lugar_cb,
                            LV_EVENT_VALUE_CHANGED, NULL);
    }

    s_parada_precio_row = make_parada_precio_row(form);

    lv_obj_t *acciones = lv_obj_create(form);
    lv_obj_set_size(acciones, lv_pct(100), 50);
    lv_obj_set_style_bg_opa(acciones, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(acciones, 0, 0);
    lv_obj_set_style_pad_all(acciones, 0, 0);
    lv_obj_set_style_pad_column(acciones, 8, 0);
    lv_obj_clear_flag(acciones, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(acciones, LV_FLEX_FLOW_ROW);

    s_parada_servicios_btn = lv_btn_create(acciones);
    lv_obj_set_height(s_parada_servicios_btn, 50);
    lv_obj_set_flex_grow(s_parada_servicios_btn, 1);
    lv_obj_set_style_bg_color(s_parada_servicios_btn, lv_color_hex(COL_VIAJE), 0);
    lv_obj_set_style_bg_color(s_parada_servicios_btn,
                              lv_color_darken(lv_color_hex(COL_VIAJE), LV_OPA_30),
                              LV_STATE_PRESSED);
    lv_obj_add_event_cb(s_parada_servicios_btn, servicios_open_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *serv_lbl = lv_label_create(s_parada_servicios_btn);
    lv_label_set_text(serv_lbl, "Servicios " LV_SYMBOL_RIGHT);
    lv_obj_set_style_text_font(serv_lbl, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(serv_lbl, lv_color_hex(COL_TILE_FG), 0);
    lv_obj_center(serv_lbl);

    lv_obj_t *guardar = make_save_button(acciones, "Guardar parada",
                                         save_generic_cb, (void *)(uintptr_t)CAT_PARADA);
    lv_obj_set_flex_grow(guardar, 2);
}

static void build_servicios(lv_obj_t *form)
{
    /* "SERVICIOS" a secas: "SERVICIOS DEL AREA" no cabe sin pisar el Volver
     * (ver el tope de add_header). No hace falta el "del area": aqui se llega
     * desde el boton Servicios de la parada, que solo sale al marcar Area. */
    add_header(form, "SERVICIOS", lv_color_hex(COL_VIAJE), CAT_PARADA);

    /* El texto lo pone servicios_open_cb segun sea area o camping. */
    s_serv_hint = lv_label_create(form);
    lv_label_set_text(s_serv_hint, "");
    lv_obj_set_style_text_color(s_serv_hint, lv_color_hex(COL_LABEL), 0);
    lv_obj_set_style_text_font(s_serv_hint, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_align(s_serv_hint, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_width(s_serv_hint, lv_pct(100));

    /* Sin boton de guardar: lo marcado aqui se guarda con la parada. El Volver
     * de la cabecera devuelve a ella con las casillas puestas.
     *
     * Con hueco ancho entre filas: esta pantalla solo lleva cabecera, rotulo y
     * seis casillas, o sea 48+20+~150 de los 304 utiles. Sobraban casi 90 px
     * en negro al final, asi que se reparten entre las filas -- mas separacion
     * es menos fallo al tocar con la autocaravana en marcha. */
    lv_obj_t *grid = make_check_grid(form, SERV_OPCIONES, SERV_COUNT, s_serv_chk,
                                     COL_VIAJE, SERV_CHK_GAP);
    /* Y el bloque, centrado en lo que sobre en vez de pegado arriba. */
    lv_obj_set_flex_align(lv_obj_get_parent(grid), LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER);

    /* La ultima casilla no marca nada: abre la pantalla de valoracion. */
    lv_obj_add_event_cb(s_serv_chk[SERV_IDX_VALORACION], valoracion_toggle_cb,
                        LV_EVENT_VALUE_CHANGED, NULL);
}

/* Pantalla de valoracion. Tres botones de un dedo con su color -- verde,
 * ambar, rojo -- y debajo las dos pegas del sitio.
 *
 * Reparto de los 304 utiles: cabecera 48 + tres botones que se reparten lo que
 * sobra (~66 cada uno) + la fila de casillas 40 + 12 de huecos. Los botones son
 * elasticos a proposito: si algun dia se anade una nota, encogen solos en vez
 * de salirse. */
static void build_valoracion(lv_obj_t *form)
{
    add_header(form, "VALORACION", lv_color_hex(COL_VIAJE), CAT_SERVICIOS);

    for (uint8_t i = 0; i < VALORACION_COUNT; i++) {
        lv_obj_t *btn = lv_btn_create(form);
        lv_obj_set_width(btn, lv_pct(100));
        lv_obj_set_height(btn, LV_SIZE_CONTENT);
        lv_obj_set_style_min_height(btn, 56, 0);
        lv_obj_set_flex_grow(btn, 1);
        lv_obj_set_style_radius(btn, 12, 0);
        lv_obj_set_style_bg_color(btn, lv_color_hex(VALORACION_COL[i]), 0);
        lv_obj_set_style_border_color(btn, lv_color_hex(0xFFFFFF), 0);
        lv_obj_add_event_cb(btn, valoracion_nota_cb, LV_EVENT_CLICKED,
                            (void *)(uintptr_t)i);

        lv_obj_t *lbl = lv_label_create(btn);
        lv_obj_set_style_text_font(lbl, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(lbl, lv_color_hex(COL_TILE_FG), 0);
        lv_obj_center(lbl);

        s_val_btn[i]     = btn;
        s_val_btn_lbl[i] = lbl;
    }

    /* Las pegas van juntas en una fila, en su propia rejilla de dos: no son
     * notas, sino cosas que pueden pasar con cualquiera de las tres. */
    make_check_grid(form, VAL_EXTRAS, VAL_EXTRA_COUNT, s_val_extra_chk,
                    COL_VIAJE, CHK_GAP);

    valoracion_pinta();
}

/* === Menus de la salida ===================================================
 *
 * El cuaderno se organiza alrededor de la SALIDA y no de las categorias. Ver
 * docs/superpowers/specs/2026-08-23-pantalla-registros-salidas-design.md; en
 * corto: la autocaravana se mueve por un motivo, y el viaje es solo uno de
 * ellos. Y como la pantalla se apaga con el contacto, DECLARAS AL LLEGAR y
 * RELLENAS AL SALIR.
 *
 * Un boton grande por pantalla: lo que se hace siempre tiene que verse desde
 * lejos y acertarse con el dedo con la autocaravana en marcha. Configuracion
 * queda pequeno y gris en todas partes.
 *
 * Los fondos CLAROS con contenido negro no son un capricho estetico: ver el
 * comentario de los colores al principio del fichero. Las tarjetas oscuras se
 * probaron y se veian apagadas con sol de lado.
 */

typedef enum {
    PAN_PRINCIPAL = 0,   /* Nueva salida / Configuracion */
    PAN_TIPO,            /* Viaje / Puntual */
    PAN_SALIDA,          /* Anadir parada / Terminar salida / Configuracion */
    PAN_TIPOS,           /* las seis cosas que se anotan en un viaje */
    PAN_PUNTUAL,         /* las cuatro de una salida puntual */
    PAN_MOTIVO,          /* por que paras */
    PAN_SITIO,           /* donde pasas la noche */
    PAN_ABIERTOS,        /* lo que queda sin cerrar, para poder borrarlo */
    PAN_COUNT
} pantalla_t;

/* Dos destinos de la flecha que NO son pantallas. Van detras de PAN_COUNT para
 * no ocupar sitio en los arrays. */
#define PAN_CANCELA_PUNTUAL  (PAN_COUNT)       /* la flecha cancela, no navega */
#define PAN_VOLVER           (PAN_COUNT + 1)   /* al menu que toque por estado */

static lv_obj_t *s_menus[PAN_COUNT];
static lv_obj_t *s_bar_hora[PAN_COUNT];
static lv_obj_t *s_bar_gps[PAN_COUNT];
static lv_obj_t *s_bar_wifi[PAN_COUNT];
/* La tira de estado. Dos, porque las dos pantallas que hacen de menu principal
 * de una salida la llevan: la de viaje y la de puntual. */
#define TIRA_SALIDA   0
#define TIRA_PUNTUAL  1
static lv_obj_t *s_tiras[2];

/* Filas de PAN_ABIERTOS: se crean las cuatro y se ocultan las que sobren, que
 * es mas simple y mas seguro que crearlas y destruirlas cada vez. */
static lv_obj_t *s_ab_fila[SALIDA_EVENTOS_MAX];
static lv_obj_t *s_ab_tipo[SALIDA_EVENTOS_MAX];
static lv_obj_t *s_ab_hora[SALIDA_EVENTOS_MAX];

#define BAR_H     26
#define PAN_PAD   12
#define PAN_GAP   10
#define CONN_MS   5000   /* mismo criterio que view_info.c */

static void mostrar_menu(pantalla_t p);
static void puntual_cancelar_cb(lv_event_t *e);
static void deshacer_ultimo(void *ud);
static void volver_al_menu(void);

/* --- La franja de arriba -------------------------------------------------
 *
 * Cuesta 26 px de los 320 y ahorra cambiar de pantalla para mirar la hora o si
 * hay enlace con la P4. Los dos puntos CADUCAN con el enlace: un indicador que
 * miente cuando se cae la comunicacion es peor que no tenerlo, porque el
 * momento en que se mira es justo cuando algo va mal. */
static lv_obj_t *punto_crear(lv_obj_t *padre, const char *texto)
{
    lv_obj_t *w = lv_obj_create(padre);
    lv_obj_remove_style_all(w);
    lv_obj_set_size(w, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(w, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(w, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(w, 4, 0);
    lv_obj_clear_flag(w, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *dot = lv_obj_create(w);
    lv_obj_set_size(dot, 8, 8);
    lv_obj_set_style_radius(dot, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x666666), 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *l = lv_label_create(w);
    lv_label_set_text(l, texto);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(0x888888), 0);
    return dot;
}

/* PAN_COUNT como destino NO es navegar: cancela la salida puntual. Es la
 * unica pantalla desde la que la flecha no lleva a ningun sitio, porque la
 * salida ya esta abierta y no hay menu anterior al que volver. */
static void atras_cb(lv_event_t *e)
{
    unsigned destino = (unsigned)(uintptr_t)lv_event_get_user_data(e);
    if (destino == PAN_CANCELA_PUNTUAL)   puntual_cancelar_cb(e);
    else if (destino == PAN_VOLVER)       volver_al_menu();
    else                                  mostrar_menu((pantalla_t)destino);
}

/* Crea una pantalla de menu entera (oculta) y devuelve su CUERPO, que es donde
 * se cuelga el contenido. 'atras' < 0 = sin boton de volver. */
static lv_obj_t *pantalla_crear(lv_obj_t *parent, pantalla_t id,
                                const char *titulo, int atras)
{
    lv_obj_t *scr = lv_obj_create(parent);
    lv_obj_remove_style_all(scr);
    lv_obj_set_size(scr, lv_pct(100), lv_pct(100));
    lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(scr, LV_FLEX_FLOW_COLUMN);
    lv_obj_add_flag(scr, LV_OBJ_FLAG_HIDDEN);
    s_menus[id] = scr;

    /* --- franja --- */
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_remove_style_all(bar);
    lv_obj_set_size(bar, lv_pct(100), BAR_H);
    lv_obj_set_style_pad_hor(bar, 10, 0);
    lv_obj_clear_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(bar, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(bar, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_column(bar, 10, 0);

    if (atras >= 0) {
        lv_obj_t *b = lv_label_create(bar);
        lv_label_set_text(b, LV_SYMBOL_LEFT " Atras");
        lv_obj_set_style_text_font(b, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(b, lv_color_hex(0xB0BEC5), 0);
        /* Area de toque generosa: el rotulo solo son 60x14 px y con la
         * autocaravana en marcha eso no se acierta. */
        lv_obj_set_ext_click_area(b, 14);
        lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
        lv_obj_add_event_cb(b, atras_cb, LV_EVENT_CLICKED, (void *)(uintptr_t)atras);
    } else {
        s_bar_hora[id] = lv_label_create(bar);
        lv_label_set_text(s_bar_hora[id], "--:--");
        lv_obj_set_style_text_font(s_bar_hora[id], &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(s_bar_hora[id], lv_color_hex(0xDDDDDD), 0);
    }

    if (titulo) {
        lv_obj_t *t = lv_label_create(bar);
        lv_label_set_text(t, titulo);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(0x888888), 0);
        lv_obj_set_style_text_letter_space(t, 1, 0);
        lv_obj_set_flex_grow(t, 1);
        lv_obj_set_style_text_align(t, LV_TEXT_ALIGN_CENTER, 0);
    } else {
        lv_obj_t *sp = lv_obj_create(bar);
        lv_obj_remove_style_all(sp);
        lv_obj_set_height(sp, 1);
        lv_obj_set_flex_grow(sp, 1);
    }

    s_bar_gps[id]  = punto_crear(bar, "GPS");
    s_bar_wifi[id] = punto_crear(bar, "P4");

    /* --- cuerpo --- */
    lv_obj_t *body = lv_obj_create(scr);
    lv_obj_remove_style_all(body);
    lv_obj_set_width(body, lv_pct(100));
    lv_obj_set_flex_grow(body, 1);
    lv_obj_set_style_pad_all(body, PAN_PAD, 0);
    lv_obj_clear_flag(body, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(body, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_style_pad_row(body, PAN_GAP, 0);
    return body;
}

/* Refresca hora y puntos de TODAS las pantallas de menu. Una sola pasada por
 * segundo: son siete etiquetas, no compensa afinar mas. */
static void barra_timer_cb(lv_timer_t *t)
{
    (void)t;
    mini_data_t d;
    data_model_get(&d);
    uint32_t ms     = (uint32_t)(esp_timer_get_time() / 1000);
    bool     fresco = d.has_data && (ms - d.last_update_ms < CONN_MS);

    uint32_t c_wifi = fresco ? 0x4CD964 : 0x666666;
    uint32_t c_gps  = !fresco ? 0x666666
                              : (d.gps_estado == 2) ? 0x4CD964
                              : (d.gps_estado == 1) ? 0xFF9800 : 0x666666;

    char hora[8] = "--:--";
    uint32_t ahora;
    if (reloj_ahora(&ahora)) {
        /* El epoch ya viene en hora local de la P4 (ver mini_proto.h): la
         * division entera basta y no hay que saber nada de husos. */
        snprintf(hora, sizeof(hora), "%02u:%02u",
                 (unsigned)((ahora / 3600) % 24), (unsigned)((ahora / 60) % 60));
    }

    for (int i = 0; i < PAN_COUNT; i++) {
        if (s_bar_gps[i])  lv_obj_set_style_bg_color(s_bar_gps[i],  lv_color_hex(c_gps), 0);
        if (s_bar_wifi[i]) lv_obj_set_style_bg_color(s_bar_wifi[i], lv_color_hex(c_wifi), 0);
        if (s_bar_hora[i]) lv_label_set_text(s_bar_hora[i], hora);
    }
}

/* --- Botones -------------------------------------------------------------- */

/* Boton grande: el que manda en su pantalla. Icono + rotulo + una linea de
 * apoyo opcional. */
static lv_obj_t *boton_grande(lv_obj_t *padre, const char *icono,
                              const char *texto, const char *apoyo,
                              uint32_t color, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = lv_btn_create(padre);
    lv_obj_set_width(b, lv_pct(100));
    lv_obj_set_flex_grow(b, 1);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(b, lv_color_darken(lv_color_hex(color), LV_OPA_30),
                              LV_STATE_PRESSED);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_pad_all(b, 6, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(b, 2, 0);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);

    if (icono) {
        lv_obj_t *ic = lv_label_create(b);
        lv_label_set_text(ic, icono);
        lv_obj_set_style_text_color(ic, lv_color_hex(COL_TILE_FG), 0);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_32, 0);
    }
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, texto);
    lv_obj_set_style_text_color(l, lv_color_hex(COL_TILE_FG), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_32, 0);
    if (apoyo) {
        lv_obj_t *s = lv_label_create(b);
        lv_label_set_text(s, apoyo);
        lv_obj_set_style_text_color(s, lv_color_hex(COL_TILE_FG), 0);
        lv_obj_set_style_text_opa(s, LV_OPA_70, 0);
        lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
        lv_obj_set_style_text_align(s, LV_TEXT_ALIGN_CENTER, 0);
    }
    return b;
}

/* Boton pequeno de abajo. Alto fijo: no debe competir con el grande. */
static lv_obj_t *boton_chico(lv_obj_t *padre, const char *texto, uint32_t color,
                             lv_coord_t ancho, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = lv_btn_create(padre);
    lv_obj_set_size(b, ancho, 46);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(b, lv_color_darken(lv_color_hex(color), LV_OPA_30),
                              LV_STATE_PRESSED);
    lv_obj_set_style_radius(b, 10, 0);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);

    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, texto);
    lv_obj_set_style_text_color(l, lv_color_hex(COL_TILE_FG), 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_center(l);
    return b;
}

/* Casilla de rejilla. 'icono' puede ser NULL: en las pantallas de motivo y de
 * sitio no lo lleva, porque la letra del firmware no trae ningun simbolo que
 * signifique "cenar" o "camping" y un icono forzado confunde mas que ayuda.
 * Sin icono, el rotulo va en letra 22 en vez de 16 y se lee mejor. */
static lv_obj_t *casilla(lv_obj_t *padre, const char *icono, const char *texto,
                         const char *apoyo, uint32_t color,
                         lv_coord_t w, lv_coord_t h, lv_event_cb_t cb, void *ud)
{
    lv_obj_t *b = lv_btn_create(padre);
    lv_obj_set_size(b, w, h);
    lv_obj_set_style_bg_color(b, lv_color_hex(color), 0);
    lv_obj_set_style_bg_color(b, lv_color_darken(lv_color_hex(color), LV_OPA_30),
                              LV_STATE_PRESSED);
    lv_obj_set_style_radius(b, 12, 0);
    lv_obj_set_style_pad_all(b, 4, 0);
    lv_obj_set_flex_flow(b, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(b, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(b, 2, 0);
    if (cb) lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, ud);

    if (icono) {
        lv_obj_t *ic = lv_label_create(b);
        lv_label_set_text(ic, icono);
        lv_obj_set_style_text_color(ic, lv_color_hex(COL_TILE_FG), 0);
        lv_obj_set_style_text_font(ic, &lv_font_montserrat_32, 0);
    }
    lv_obj_t *l = lv_label_create(b);
    lv_label_set_text(l, texto);
    lv_obj_set_style_text_color(l, lv_color_hex(COL_TILE_FG), 0);
    lv_obj_set_style_text_font(l, icono ? &lv_font_montserrat_16 : &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_WRAP);
    lv_obj_set_width(l, lv_pct(100));

    if (apoyo) {
        lv_obj_t *s = lv_label_create(b);
        lv_label_set_text(s, apoyo);
        lv_obj_set_style_text_color(s, lv_color_hex(COL_TILE_FG), 0);
        lv_obj_set_style_text_opa(s, LV_OPA_70, 0);
        lv_obj_set_style_text_font(s, &lv_font_montserrat_14, 0);
    }
    return b;
}

/* Fila elastica para repartir casillas a lo ancho. */
static lv_obj_t *fila(lv_obj_t *padre)
{
    lv_obj_t *f = lv_obj_create(padre);
    lv_obj_remove_style_all(f);
    lv_obj_set_width(f, lv_pct(100));
    lv_obj_set_flex_grow(f, 1);
    lv_obj_clear_flag(f, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_style_pad_column(f, PAN_GAP, 0);
    return f;
}

/* === Que hace cada boton ==================================================
 *
 * Declarar es lo UNICO que se hace al llegar: se abre el evento y se vuelve al
 * menu. Los numeros (importe, litros, precio de la noche...) se piden al
 * arrancar, que es cuando ya se saben -- esas pantallas todavia no estan
 * hechas, ver el diseno.
 */

static const char *const EV_NOMBRE[EV_COUNT] = {
    "Parada", "Aguas", "Repostaje", "Peaje", "Bombona", "Averia", "ITV"
};

/* Todas las pantallas de menu ocultas. Lo llama show_form() antes de sacar un
 * formulario. */
static void ocultar_menus(void)
{
    for (int i = 0; i < PAN_COUNT; i++) {
        if (s_menus[i]) lv_obj_add_flag(s_menus[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/* La tira de PAN_SALIDA: nombre y dia del viaje, y cuantas cosas quedan sin
 * cerrar.
 *
 * NO lleva lo gastado, aunque la maqueta lo pintaba: esta pantalla no lleva la
 * cuenta del dinero -- los importes se rellenan al arrancar y viven en la P4 --
 * y poner un 0,00 seria mentir con autoridad. */
/* "hh:mm" a partir de un epoch local de la P4. */
static void hora_corta(uint32_t epoch, char *buf, size_t n)
{
    snprintf(buf, n, "%02u:%02u", (unsigned)((epoch / 3600) % 24),
             (unsigned)((epoch / 60) % 60));
}

static void salida_tira_refresh(void)
{
    const salida_vista_t *s = salida_get();
    char txt[SALIDA_NOMBRE_MAX + 32];
    int  n = snprintf(txt, sizeof(txt), "%s", s->nombre);
    /* snprintf devuelve lo que HABRIA escrito: si truncase, txt+n se saldria
     * del buffer. Con los tamanos de ahora no llega a pasar, pero el dia que
     * el nombre crezca esto seria una pisada de memoria muy dificil de ver. */
    if (n < 0 || (size_t)n >= sizeof(txt)) return;

    uint32_t ahora = reloj_p4();
    if (ahora && s->epoch_ini) {
        n += snprintf(txt + n, sizeof(txt) - (size_t)n, "  -  dia %u",
                      (unsigned)(salida_noches(s->epoch_ini, ahora) + 1));
        if (n < 0 || (size_t)n >= sizeof(txt)) return;
    }
    int abiertos = salida_eventos_abiertos();
    if (abiertos > 0) {
        /* La flecha no es adorno: avisa de que la tira se puede tocar, que es
         * por donde se llega a borrar un apunte puesto por error. */
        snprintf(txt + n, sizeof(txt) - (size_t)n, "  -  %d sin cerrar  >", abiertos);
    }
    for (int i = 0; i < 2; i++) {
        if (s_tiras[i]) lv_label_set_text(s_tiras[i], txt);
    }
}

/* Pinta la lista de lo que queda abierto. Las filas que sobran se esconden. */
static void abiertos_refresh(void)
{
    int n = salida_eventos_abiertos();
    for (int i = 0; i < SALIDA_EVENTOS_MAX; i++) {
        if (!s_ab_fila[i]) continue;
        const salida_evento_t *ev = salida_evento_en(i);
        if (!ev) { lv_obj_add_flag(s_ab_fila[i], LV_OBJ_FLAG_HIDDEN); continue; }
        lv_obj_clear_flag(s_ab_fila[i], LV_OBJ_FLAG_HIDDEN);
        lv_label_set_text(s_ab_tipo[i],
                          ev->tipo < EV_COUNT ? EV_NOMBRE[ev->tipo] : "?");
        char h[8];
        hora_corta(ev->epoch_ini, h, sizeof(h));
        char linea[32];
        snprintf(linea, sizeof(linea), "anotado a las %s", h);
        lv_label_set_text(s_ab_hora[i], linea);
    }
    (void)n;
}

static void mostrar_menu(pantalla_t p)
{
    salida_tira_refresh();
    if (p == PAN_ABIERTOS) abiertos_refresh();
    for (int i = 0; i < PAN_COUNT; i++) {
        if (!s_menus[i]) continue;
        if (i == (int)p) lv_obj_clear_flag(s_menus[i], LV_OBJ_FLAG_HIDDEN);
        else             lv_obj_add_flag(s_menus[i], LV_OBJ_FLAG_HIDDEN);
    }
}

/* A donde se vuelve al cerrar un formulario o al terminar algo. Lo decide el
 * ESTADO y no por donde se vino: si la salida se abrio o se cerro por el medio,
 * el menu de antes ya no es el que toca. */
static void volver_al_menu(void)
{
    switch (salida_get()->tipo) {
    case SALIDA_VIAJE:   mostrar_menu(PAN_SALIDA);    break;
    case SALIDA_PUNTUAL: mostrar_menu(PAN_PUNTUAL);   break;
    default:             mostrar_menu(PAN_PRINCIPAL); break;
    }
}

/* Abre el evento y vuelve al menu. Los tres motivos por los que puede no
 * poder se dicen por separado: "no se ha podido" a secas deja al usuario sin
 * saber si insistir, encender la P4 o cerrar algo. */
static void declarar(evento_tipo_t tipo, uint8_t sub, uint8_t sub2)
{
    if (reloj_p4() == 0) {
        confirm_screen_aviso("Enciende la P4 primero",
                             "Sin ella no se que hora es,\ny el apunte va con su hora\nde inicio.",
                             COL_ACCION_STOP, "Entendido");
        return;
    }
    if (salida_eventos_abiertos() >= SALIDA_EVENTOS_MAX) {
        confirm_screen_aviso("Ya hay cuatro sin cerrar",
                             "No cabe otra. Te preguntare\npor ellas cuando vuelvas a\ndar el contacto.",
                             COL_ACCION_STOP, "Entendido");
        return;
    }
    if (salida_evento_abrir(tipo, sub, sub2) == 0) {
        confirm_screen_aviso("No he podido anotarlo",
                             "El apunte no se ha guardado.\nAvisa de esto, es un fallo\ndel programa.",
                             COL_ACCION_STOP, "Entendido");
        return;
    }

    /* El cartel no es solo para enterarse: lleva DESHACER. El evento se abre de
     * un toque, y sin esto un dedo equivocado no tendria vuelta atras. Va en
     * rojo y a la derecha, que es donde estan el resto de acciones que
     * descartan. */
    static char cuerpo[96];
    snprintf(cuerpo, sizeof(cuerpo),
             "Queda abierto: %s.\nCuando vuelvas a dar el\ncontacto te pedire los datos.",
             EV_NOMBRE[tipo]);
    confirm_screen_open("Anotado", cuerpo, COL_ACCION_STOP,
                        "Deshacer", "Vale", deshacer_ultimo, NULL);
    volver_al_menu();
}

/* tipo, sub y sub2 caben de sobra en el user_data del evento. */
#define DECL(t, s, s2)  ((void *)(uintptr_t)((t) | ((s) << 8) | ((s2) << 16)))

static void declarar_cb(lv_event_t *e)
{
    uint32_t v = (uint32_t)(uintptr_t)lv_event_get_user_data(e);
    declarar((evento_tipo_t)(v & 0xFF), (uint8_t)((v >> 8) & 0xFF),
             (uint8_t)((v >> 16) & 0xFF));
}

static void ir_a_cb(lv_event_t *e)
{
    mostrar_menu((pantalla_t)(uintptr_t)lv_event_get_user_data(e));
}

/* Tocar la tira solo lleva a algun sitio si hay algo que ver. Sin nada abierto
 * no hace nada, en vez de abrir una lista vacia. */
static void tira_cb(lv_event_t *e)
{
    (void)e;
    if (salida_eventos_abiertos() > 0) mostrar_menu(PAN_ABIERTOS);
}

/* --- Borrar un apunte puesto por error ------------------------------------ */

static char s_borrar_txt[96];

static void borrar_do(void *ud)
{
    salida_evento_borrar((int)(intptr_t)ud);
    if (salida_eventos_abiertos() == 0) volver_al_menu();   /* ya no hay lista */
    else                                { abiertos_refresh(); salida_tira_refresh(); }
}

static void borrar_cb(lv_event_t *e)
{
    int idx = (int)(intptr_t)lv_event_get_user_data(e);
    const salida_evento_t *ev = salida_evento_en(idx);
    if (!ev) return;
    char h[8];
    hora_corta(ev->epoch_ini, h, sizeof(h));
    snprintf(s_borrar_txt, sizeof(s_borrar_txt),
             "%s, anotado a las %s.\nNo se guardara nada de el.",
             ev->tipo < EV_COUNT ? EV_NOMBRE[ev->tipo] : "?", h);
    confirm_screen_open("Borrar el apunte?", s_borrar_txt, COL_ACCION_STOP,
                        "Si, borrar", "No, dejarlo", borrar_do,
                        (void *)(intptr_t)idx);
}

/* Deshacer lo que se acaba de anotar: el ultimo de la cola. */
static void deshacer_ultimo(void *ud)
{
    (void)ud;
    salida_evento_borrar(salida_eventos_abiertos() - 1);
    volver_al_menu();
}

/* --- Abrir y cerrar la salida --------------------------------------------- */

static void puntual_cb(lv_event_t *e)
{
    (void)e;
    /* Igual que el viaje: se exige la P4 ANTES de nada, porque el apunte va
     * con su hora y este aparato no tiene reloj propio. */
    if (reloj_p4() == 0) {
        confirm_screen_aviso("Enciende la P4 primero",
                             "Sin ella no se que hora es,\ny el apunte va con su hora.",
                             COL_ACCION_STOP, "Entendido");
        return;
    }
    if (!salida_abrir_puntual()) {
        confirm_screen_aviso("No he podido empezarla",
                             "La salida no se ha guardado.\nAvisa de esto, es un fallo\ndel programa.",
                             COL_ACCION_STOP, "Entendido");
        return;
    }
    mostrar_menu(PAN_PUNTUAL);
}

/* Cerrar la salida OLVIDA lo que quedase abierto (ver salida_cerrar). Cuando
 * hay algo abierto no se impide -- si se impidiera no habria forma de salir,
 * porque cerrar un apunte solo se puede al arrancar y esa parte aun no esta --
 * pero se dice CUANTO se pierde y se pide el segundo toque.
 *
 * Buffer estatico porque el cartel lo lee mientras esta abierto. */
static char s_perder_txt[80];

static bool avisa_de_lo_abierto(const char *titulo, confirm_cb_t si)
{
    int n = salida_eventos_abiertos();
    if (n == 0) return false;
    snprintf(s_perder_txt, sizeof(s_perder_txt),
             "Hay %d apunte%s sin cerrar.\nSi sigues, se pierde%s.",
             n, n == 1 ? "" : "s", n == 1 ? "" : "n");
    confirm_screen_open(titulo, s_perder_txt, COL_ACCION_STOP,
                        "Si, descartar", "No, dejarlo", si, NULL);
    return true;
}

static void puntual_do_cancelar(void *ud)
{
    (void)ud;
    salida_cerrar();
    mostrar_menu(PAN_PRINCIPAL);
}

/* En una salida puntual la flecha de arriba no navega: la CANCELA. No hay a
 * donde volver -- la salida ya esta abierta -- asi que deshacerla es lo unico
 * que tiene sentido. */
static void puntual_cancelar_cb(lv_event_t *e)
{
    (void)e;
    if (avisa_de_lo_abierto("Cancelar la salida?", puntual_do_cancelar)) return;
    puntual_do_cancelar(NULL);
}

static void terminar_salida_cb(lv_event_t *e)
{
    (void)e;
    if (avisa_de_lo_abierto("Terminar el viaje?", viaje_do_finalizar)) return;
    viaje_finalizar_cb(e);   /* el mismo cartel de confirmacion de siempre */
}

/* --- Las siete pantallas --------------------------------------------------
 *
 * Geometria (480x320): la franja se come 26, quedan 294; con PAN_PAD de 12 por
 * lado el cuerpo util es 456 x 270. De ahi salen los tamanos:
 *
 *   rejilla de 3 columnas: (456 - 2*10)/3 = 145 de ancho
 *   rejilla de 2 columnas: (456 - 10)/2   = 223
 *   dos filas:             (270 - 10)/2   = 130 de alto
 *
 * No van en porcentaje: en LVGL el hueco entre celdas NO se descuenta del
 * porcentaje y la tercera columna se caeria de fila (ver el bloque del menu
 * viejo, mismo motivo).
 */
#define CEL3_W  145
#define CEL2_W  223

/* La tira de estado de un menu de salida. Es un rotulo, pero se TOCA: es la
 * puerta a la lista de lo que queda abierto. Area de toque generosa por lo de
 * siempre -- 16 px de alto no se aciertan con el vehiculo en marcha. */
static lv_obj_t *tira_crear(lv_obj_t *body)
{
    lv_obj_t *l = lv_label_create(body);
    lv_label_set_text(l, "");
    lv_obj_set_style_text_font(l, &lv_font_montserrat_16, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(COL_LABEL), 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_DOT);
    lv_obj_set_width(l, lv_pct(100));
    lv_obj_add_flag(l, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_ext_click_area(l, 12);
    lv_obj_add_event_cb(l, tira_cb, LV_EVENT_CLICKED, NULL);
    return l;
}

/* Una fila de la lista de lo abierto: que es, a que hora se anoto, y Borrar. */
static void abiertos_fila_crear(lv_obj_t *body, int idx)
{
    lv_obj_t *f = lv_obj_create(body);
    lv_obj_remove_style_all(f);
    lv_obj_set_width(f, lv_pct(100));
    lv_obj_set_height(f, 58);
    lv_obj_clear_flag(f, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(f, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(f, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_add_flag(f, LV_OBJ_FLAG_HIDDEN);
    s_ab_fila[idx] = f;

    lv_obj_t *col = lv_obj_create(f);
    lv_obj_remove_style_all(col);
    lv_obj_set_height(col, LV_SIZE_CONTENT);
    lv_obj_set_flex_grow(col, 1);
    lv_obj_clear_flag(col, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(col, LV_FLEX_FLOW_COLUMN);

    s_ab_tipo[idx] = lv_label_create(col);
    lv_obj_set_style_text_font(s_ab_tipo[idx], &lv_font_montserrat_22, 0);
    lv_obj_set_style_text_color(s_ab_tipo[idx], lv_color_hex(0xFFFFFF), 0);

    s_ab_hora[idx] = lv_label_create(col);
    lv_obj_set_style_text_font(s_ab_hora[idx], &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_ab_hora[idx], lv_color_hex(COL_LABEL), 0);

    boton_chico(f, "Borrar", COL_ACCION_STOP, 120, borrar_cb, (void *)(intptr_t)idx);
}

static void crear_menus(lv_obj_t *parent)
{
    lv_obj_t *body, *f;

    /* --- 1. Principal: sin salida en marcha --- */
    body = pantalla_crear(parent, PAN_PRINCIPAL, NULL, -1);
    lv_obj_set_flex_align(body, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);
    boton_grande(body, LV_SYMBOL_PLUS, "NUEVA SALIDA", "viaje o gestion suelta",
                 COL_ACCION_OK, ir_a_cb, (void *)(uintptr_t)PAN_TIPO);
    boton_chico(body, "Configuracion", COL_AJUSTES, 160, ajustes_click_cb, NULL);

    /* --- 2. Tipo de salida --- */
    body = pantalla_crear(parent, PAN_TIPO, "TIPO DE SALIDA", PAN_PRINCIPAL);
    f = fila(body);
    /* Las dos del mismo tamano: ninguna manda sobre la otra. */
    casilla(f, LV_SYMBOL_GPS, "VIAJE", "varios dias\ncon carpeta propia",
            COL_VIAJE, CEL2_W, lv_pct(100), viaje_iniciar_cb, NULL);
    casilla(f, LV_SYMBOL_CHARGE, "PUNTUAL", "repostar, ITV,\nbombona o taller",
            COL_BOMBONA, CEL2_W, lv_pct(100), puntual_cb, NULL);

    /* --- 3. Menu de salida: el principal mientras dure el viaje --- */
    body = pantalla_crear(parent, PAN_SALIDA, NULL, -1);
    s_tiras[TIRA_SALIDA] = tira_crear(body);

    boton_grande(body, LV_SYMBOL_PLUS, "ANADIR PARADA", NULL,
                 COL_ACCION_OK, ir_a_cb, (void *)(uintptr_t)PAN_TIPOS);

    /* Los dos secundarios comparten fila: entre los dos gastan lo que gastaria
     * uno solo, y asi el grande se queda con el sitio. */
    f = fila(body);
    lv_obj_set_flex_grow(f, 0);
    lv_obj_set_height(f, 46);
    lv_obj_set_flex_grow(boton_chico(f, "Terminar salida", COL_ACCION_STOP, 0,
                                     terminar_salida_cb, NULL), 1);
    lv_obj_set_flex_grow(boton_chico(f, "Configuracion", COL_AJUSTES, 0,
                                     ajustes_click_cb, NULL), 1);

    /* --- 4. Las seis cosas que se anotan en un viaje --- */
    body = pantalla_crear(parent, PAN_TIPOS, "QUE ANOTAS?", PAN_SALIDA);
    f = fila(body);
    casilla(f, LV_SYMBOL_GPS,      "Parada",    NULL, COL_ACCION_OK,
            CEL3_W, lv_pct(100), ir_a_cb, (void *)(uintptr_t)PAN_MOTIVO);
    casilla(f, LV_SYMBOL_TINT,     "Aguas",     NULL, COL_VIAJE,
            CEL3_W, lv_pct(100), declarar_cb, DECL(EV_AGUAS, 0, 0));
    casilla(f, LV_SYMBOL_CHARGE,   "Repostaje", NULL, COL_BOMBONA,
            CEL3_W, lv_pct(100), declarar_cb, DECL(EV_REPOSTAJE, 0, 0));
    f = fila(body);
    /* El peaje es la excepcion: se paga con el motor en marcha y lo rellena el
     * copiloto en el momento, asi que abre su formulario y no un evento. */
    casilla(f, LV_SYMBOL_LIST,     "Peaje",     NULL, COL_PEAJE,
            CEL3_W, lv_pct(100), icon_click_cb, (void *)(uintptr_t)CAT_PEAJE);
    casilla(f, LV_SYMBOL_REFRESH,  "Bombona",   NULL, COL_AJUSTES,
            CEL3_W, lv_pct(100), declarar_cb, DECL(EV_BOMBONA, 0, 0));
    casilla(f, LV_SYMBOL_SETTINGS, "Averia",    NULL, COL_AJUSTES,
            CEL3_W, lv_pct(100), declarar_cb, DECL(EV_AVERIA, 0, 0));

    /* --- 5. Las cuatro de una salida puntual --- */
    body = pantalla_crear(parent, PAN_PUNTUAL, "SALIDA PUNTUAL", PAN_CANCELA_PUNTUAL);
    s_tiras[TIRA_PUNTUAL] = tira_crear(body);
    f = fila(body);
    casilla(f, LV_SYMBOL_CHARGE,   "Repostaje", NULL, COL_BOMBONA,
            CEL2_W, lv_pct(100), declarar_cb, DECL(EV_REPOSTAJE, 0, 0));
    casilla(f, LV_SYMBOL_REFRESH,  "Bombona",   NULL, COL_AJUSTES,
            CEL2_W, lv_pct(100), declarar_cb, DECL(EV_BOMBONA, 0, 0));
    f = fila(body);
    casilla(f, LV_SYMBOL_FILE,     "ITV",       NULL, COL_VIAJE,
            CEL2_W, lv_pct(100), declarar_cb, DECL(EV_ITV, 0, 0));
    casilla(f, LV_SYMBOL_SETTINGS, "Averia/Mant.", NULL, COL_AJUSTES,
            CEL2_W, lv_pct(100), declarar_cb, DECL(EV_AVERIA, 0, 0));

    /* --- 6. Por que paras --- */
    body = pantalla_crear(parent, PAN_MOTIVO, "POR QUE PARAS?", PAN_TIPOS);
    f = fila(body);
    casilla(f, NULL, "Visita",   NULL, COL_AJUSTES, CEL3_W, lv_pct(100),
            declarar_cb, DECL(EV_PARADA, MOTIVO_VISITA, 0));
    casilla(f, NULL, "Descanso", NULL, COL_AJUSTES, CEL3_W, lv_pct(100),
            declarar_cb, DECL(EV_PARADA, MOTIVO_DESCANSO, 0));
    casilla(f, NULL, "Comer",    NULL, COL_AJUSTES, CEL3_W, lv_pct(100),
            declarar_cb, DECL(EV_PARADA, MOTIVO_COMER, 0));
    f = fila(body);
    casilla(f, NULL, "Cenar",    NULL, COL_AJUSTES, CEL3_W, lv_pct(100),
            declarar_cb, DECL(EV_PARADA, MOTIVO_CENAR, 0));
    casilla(f, NULL, "Compras",  NULL, COL_AJUSTES, CEL3_W, lv_pct(100),
            declarar_cb, DECL(EV_PARADA, MOTIVO_COMPRAS, 0));
    /* Pernocta va en verde y no en gris como las otras cinco: es la unica que
     * lleva cola -- donde, servicios, precio y valoracion -- y no debe
     * pulsarse por error creyendo que es un descanso. */
    casilla(f, NULL, "PERNOCTA", NULL, COL_ACCION_OK, CEL3_W, lv_pct(100),
            ir_a_cb, (void *)(uintptr_t)PAN_SITIO);

    /* --- 7. Donde pasas la noche --- */
    body = pantalla_crear(parent, PAN_SITIO, "DONDE DUERMES?", PAN_MOTIVO);
    f = fila(body);
    /* Verde gratis, ambar de pago: el color dice si esto va a costar dinero
     * antes de leer nada, y es lo que decide que se preguntara al marcharse. */
    casilla(f, NULL, "Parking", "gratis",  COL_ACCION_OK, CEL3_W, lv_pct(100),
            declarar_cb, DECL(EV_PARADA, MOTIVO_PERNOCTA, SITIO_PARKING_GRATIS));
    casilla(f, NULL, "Parking", "de pago", COL_BOMBONA,   CEL3_W, lv_pct(100),
            declarar_cb, DECL(EV_PARADA, MOTIVO_PERNOCTA, SITIO_PARKING_PAGO));
    casilla(f, NULL, "Area",    "gratis",  COL_ACCION_OK, CEL3_W, lv_pct(100),
            declarar_cb, DECL(EV_PARADA, MOTIVO_PERNOCTA, SITIO_AREA_GRATIS));
    f = fila(body);
    casilla(f, NULL, "Area",    "de pago", COL_BOMBONA,   CEL3_W, lv_pct(100),
            declarar_cb, DECL(EV_PARADA, MOTIVO_PERNOCTA, SITIO_AREA_PAGO));
    /* El camping ocupa lo que dos: sobra sitio y es el que mas se pulsa de los
     * de pago. 145 + 10 + 300 = 455, los 456 utiles. */
    casilla(f, NULL, "Camping", "de pago", COL_BOMBONA,   300, lv_pct(100),
            declarar_cb, DECL(EV_PARADA, MOTIVO_PERNOCTA, SITIO_CAMPING));

    /* --- 8. Lo que queda sin cerrar ---
     * Se llega tocando la tira. Existe para PODER DESHACER: hasta que estén
     * las pantallas de al volver a dar el contacto, este es el unico sitio
     * desde donde quitar un apunte puesto por error. La flecha vuelve al menu
     * que toque, que no es siempre el mismo (viaje o puntual). */
    body = pantalla_crear(parent, PAN_ABIERTOS, "SIN CERRAR", PAN_VOLVER);
    for (int i = 0; i < SALIDA_EVENTOS_MAX; i++) abiertos_fila_crear(body, i);
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

    /* --- Menus de la salida ---
     * Se crean ANTES que los formularios para que estos queden por encima en
     * el orden de dibujo. Cual se ve lo decide volver_al_menu(), al final. */
    crear_menus(parent);

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

    s_forms[CAT_PARADA] = make_form_container(parent);
    build_parada(s_forms[CAT_PARADA]);

    s_forms[CAT_SERVICIOS] = make_form_container(parent);
    build_servicios(s_forms[CAT_SERVICIOS]);

    s_forms[CAT_VALORACION] = make_form_container(parent);
    build_valoracion(s_forms[CAT_VALORACION]);

    /* Estado de partida: si se fue la luz en mitad de un viaje, sigue habiendo
     * viaje. viaje_refresh() deja la pantalla de Viaje y el rotulo de su
     * casilla acordes; parada_refresh_extras() esconde el precio y el boton de
     * servicios, que solo salen al marcar area o camping. */
    load_trip_active(&s_viaje_activo);
    viaje_refresh();
    parada_refresh_extras();

    /* Si quedo una parada sin cerrar, vigilar en segundo plano hasta que la P4
     * diga que dia es y preguntar entonces. El dialogo se muda solo a la
     * pantalla que este activa (ver confirm_screen.c), que al arrancar es la
     * principal. Cada 2 s: la fecha llega a 1 Hz y no hay ninguna prisa. */
    parada_abierta_t pendiente;
    load_parada_abierta(&pendiente);
    s_parada_abierta = pendiente.abierta;
    viaje_refresh();           /* con el dato ya cargado: saca su boton */
    if (s_parada_abierta) {
        lv_timer_create(parada_fin_timer_cb, 2000, NULL);
    }

    /* --- Editor de campo --- */
    /* Editor de campo a pantalla completa. Se crea el ULTIMO a proposito: asi
     * queda por encima de los formularios en el orden de dibujo. */
    entry_screen_init(parent);
    confirm_screen_init(parent);

    /* Hora y puntos de la franja, una pasada por segundo. */
    lv_timer_create(barra_timer_cb, 1000, NULL);

    /* El menu de partida sale del ESTADO, no de un valor por defecto: si se
     * fue la luz en mitad de un viaje, se vuelve al menu de salida y no a la
     * pantalla principal. */
    volver_al_menu();
    s_ui_lista = true;
}
