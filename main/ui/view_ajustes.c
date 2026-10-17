/* view_ajustes.c - CONFIGURACION del aparato.
 *
 * Dos pantallas en una: el MENU (que version lleva puesta y un boton para el
 * Wi-Fi) y el formulario del WI-FI. Antes "Configuración" entraba directamente
 * al formulario, y ni siquiera habia donde mirar la version -- que es el primer
 * dato que hace falta cuando algo va raro.
 *
 * Wi-Fi: SSID/password de la P4 a la que se asocia, editables en el propio
 * aparato (guardado en NVS via net/udp_rx.c). Resuelve el mismo problema que ya
 * sufrio victron_mini: password fija en el firmware, sin forma de cambiar de P4
 * sin reflashear.
 */
#include "view_ajustes.h"
#include "ui/ui_style.h"   /* la paleta, en un solo sitio */
#include "fonts/fonts_es.h"   /* Inter + acentos: ver fonts_es.h */
#include "view_info.h"
#include "../net/udp_rx.h"
#include "../net/ota_rx.h"   /* actualizacion por Wi-Fi: la lanza y se pinta aqui */
#include "../net/mini_proto.h"   /* MINI_PROTO_VERSION: el diagnostico del enlace */
#include "confirm_screen.h"
#include "config_storage.h"
#include "diag_reset.h"
#include "entry_screen.h"
#include "nav.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include <stdio.h>
#include <string.h>

/* El boton "Volver" de las dos cabeceras de este fichero (la del menu y la del
 * Wi-Fi): 80 px de ancho, pegado al borde izquierdo y separado 8 px de lo que
 * venga detras. Se saca a constantes porque el TITULO tambien tiene que saber
 * cuanto ocupa (ver CAB_TITULO_X). */
#define CAB_BACK_W    80
#define CAB_BACK_GAP  8

/* Lo que hay que dejar libre a la izquierda del titulo para no pisar el boton.
 * La pantalla son 480x320 logicos (480 de ancho, no 320: display.h + la
 * rotacion de 90 grados de main.c). El boton "Volver" ocupa de 0 a 80 y el
 * rotulo "CONFIGURACIÓN" mide 185,9 px. Los 21 px de solape que se midieron en
 * la placa salen de una fila de 304 px (la pantalla a 320 de ancho menos los
 * 8 px de pad a cada lado): (304 - 185,9) / 2 = 59, que es donde empezaba el
 * rotulo. Con los 480 de ahora el centrado a fila entera ya no llega al boton,
 * pero el pad se queda: es lo que impide que un rotulo mas largo (o un boton
 * mas ancho) acabe debajo. */
#define CAB_TITULO_X  (CAB_BACK_W + CAB_BACK_GAP)

/* Centra un titulo de cabecera en el hueco de la derecha del Volver: la
 * etiqueta ocupa la fila entera (para poder centrar el texto) y se le reserva a
 * la izquierda el ancho del boton con un pad. El pad de la etiqueta ES el hueco
 * del texto: lv_label dibuja dentro del area de contenido (lv_label.c), asi que
 * el centrado del texto sale dentro del hueco y no de la fila. */
static void titulo_cabecera(lv_obj_t *lbl)
{
    lv_obj_set_width(lbl, lv_pct(100));
    lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_pad_left(lbl, CAB_TITULO_X, 0);
    lv_obj_align(lbl, LV_ALIGN_CENTER, 0, 0);   /* TODOS los titulos van centrados (usuario, 16-oct) */
}

static const char *TAG = "view_ajustes";

/* Las dos caras de la pantalla: se ensena una y se esconde la otra. */
static lv_obj_t *s_menu;
static lv_obj_t *s_wifi;
/* La pantalla de Ajustes entera (el padre de las dos caras). Se guarda para
 * saber si esta DELANTE: el temporizador de la actualizacion no puede abrir el
 * cartel de confirmacion encima de la pantalla que el usuario este mirando. */
static lv_obj_t *s_raiz;

static lv_obj_t *s_ver_lbl;
static lv_obj_t *s_ssid_ta;
static lv_obj_t *s_enlace;        /* estado del enlace con la P4 */
static lv_obj_t *s_ota_lbl;       /* estado de la actualizacion (comparte sitio) */
static lv_timer_t *s_ota_timer;
/* Cierto mientras el cartel "hay version nueva" ya se abrio para esta oferta.
 * Sin esto, el temporizador lo abriria otra vez cada 500 ms al cancelarlo. */
static bool s_ota_preguntando;
/* El cuerpo del cartel, estatico: confirm_screen_open() se queda con el puntero
 * y lo lee despues, asi que no puede ser una variable local de la funcion. */
static char s_ota_body[96];
static lv_obj_t *s_uptime_lbl;    /* cuanto lleva en marcha (ver diag_reset) */
static lv_timer_t *s_enlace_timer;
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

/* max_len: sin esto ningun campo tenia tope, y el guardado (save_wifi_config
 * via udp_rx_set_credentials / save_portal_creds) escribe en NVS sin
 * truncar -- pero el RECARGADO si usa buffers fijos (s_ssid/s_pass en
 * udp_rx.c: 32/64; hu/hp en view_ajustes_refresh: 32/64), asi que un valor
 * mas largo tecleado aqui se guardaba bien la primera vez y luego, en la
 * siguiente recarga de esta pantalla, fallaba a leerse -- el campo aparecia
 * vacio, como si "se hubiera olvidado" la credencial. entry_screen_open ya
 * respeta lv_textarea_get_max_length() del campo origen (entry_screen_open),
 * asi que el editor a pantalla completa hereda el tope sin tocar nada mas
 * alli. Detectado por el usuario el 09-sep-2026. */
/* 'oculto': el campo se enseña con puntitos y solo se ve al abrirlo en el
 * editor. Lo llevan las DOS claves (la del Wi-Fi y la del portal) desde la
 * auditoria de seguridad del 13-oct-2026: estaban a la vista, y de pie junto a
 * la pantalla se leian de un vistazo. En el P4 ya se hacia asi. */
/* ── UN CAMPO: ROTULO A LA IZQUIERDA Y CAJA A LA DERECHA, EN LA MISMA LINEA ──
 *
 * PEDIDO POR EL USUARIO (16-oct-2026): "no has seguido el mismo criterio de
 * disposicion en la 3,5 de la pantalla configuracion wifi: ssid y nombre en la
 * misma linea y las demas tambien". La 5" ya lo tenia asi (make_field_en:
 * rotulo de 190 px con la caja a su derecha, en la misma fila) y aqui el rotulo
 * iba ENCIMA de la caja: las dos pantallas se leian distinto y cada campo
 * gastaba una linea de mas. Ahora se copia el criterio de la 5": rotulo a la
 * izquierda, con ancho FIJO para que todas las cajas empiecen en la misma x, y
 * la caja ocupando lo que sobra.
 *
 * Los numeros son fijos, como en la 5" ("posiciones a mano, sin flex"): esta
 * pantalla son 480x320 logicos, el formulario deja 8 px de margen a cada lado
 * (464 utiles), y 92 del rotulo + 8 de aire dejan 364 para la caja. En 92 px
 * entra el rotulo mas largo ("Password", 64 px medidos con la Inter 14; el
 * comentario decia 62 y no era la medida real). */
#define WF_MARGEN     8
#define WF_ROTULO_W   92
#define WF_AIRE       8
#define WF_CAMPO_H    40                                  /* el rotulo ya no ocupa linea propia */
#define WF_CAJA_W     (480 - 2 * WF_MARGEN - WF_ROTULO_W - WF_AIRE)

static lv_obj_t *make_field(lv_obj_t *parent, const char *label_text, uint32_t max_len,
                            bool oculto)
{
    lv_obj_t *cont = lv_obj_create(parent);
    lv_obj_set_size(cont, lv_pct(100), WF_CAMPO_H);
    lv_obj_set_style_bg_opa(cont, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(cont, 0, 0);
    lv_obj_set_style_pad_all(cont, 0, 0);
    lv_obj_clear_flag(cont, LV_OBJ_FLAG_SCROLLABLE);

    /* El rotulo, a la izquierda y centrado en el alto de la fila (con su tamaño
     * natural y alineado al centro izquierda cae centrado solo). */
    lv_obj_t *lbl = lv_label_create(cont);
    lv_label_set_text(lbl, label_text);
    lv_obj_set_style_text_color(lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(lbl, &lv_font_montserrat_14, 0);
    lv_obj_align(lbl, LV_ALIGN_LEFT_MID, 0, 0);

    lv_obj_t *ta = lv_textarea_create(cont);
    lv_textarea_set_one_line(ta, true);
    lv_textarea_set_max_length(ta, max_len);
    if (oculto) lv_textarea_set_password_mode(ta, true);
    lv_obj_set_size(ta, WF_CAJA_W, 34);
    lv_obj_align(ta, LV_ALIGN_RIGHT_MID, 0, 0);
    /* En CLICKED y no en FOCUSED: al volver del editor el campo conserva el
     * foco, asi que con FOCUSED el segundo toque no volveria a abrirlo. El
     * rotulo viaja en el user_data y es el titulo que saca el editor. */
    lv_obj_set_user_data(ta, (void *)label_text);
    lv_obj_add_event_cb(ta, ta_click_cb, LV_EVENT_CLICKED, NULL);

    return ta;
}

/* Cabecera de grupo: dice QUE es cada bloque de campos. Mismo criterio (y
 * mismos textos) que la 5": antes aqui eran dos parrafos de ayuda sueltos, que
 * se comian 58 px de alto y se leian peor porque no estaban pegados a lo que
 * explicaban. */
static void make_grupo(lv_obj_t *parent, const char *texto)
{
    lv_obj_t *l = lv_label_create(parent);
    lv_label_set_text(l, texto);
    lv_obj_set_style_text_color(l, UI_COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
    lv_obj_set_width(l, lv_pct(100));
}

static void mostrar_menu(bool menu)
{
    if (!s_menu || !s_wifi) return;
    if (menu) {
        lv_obj_clear_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_add_flag(s_wifi, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_add_flag(s_menu, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_wifi, LV_OBJ_FLAG_HIDDEN);
    }
}

/* Estado del guardado del Wi-Fi, con el COLOR del resultado (D4).
 *
 * POR QUE: s_status_lbl nacia en verde y se quedaba en verde tambien cuando el
 * guardado fallaba o faltaba el SSID; el usuario leia "Falta el SSID" en verde
 * de exito. Aqui se cambia el texto y el color a la vez, para que no haya una
 * salida que se deje uno de los dos. Verde = se guardo; rojo = no. */
static void wifi_estado(const char *txt, bool ok)
{
    if (!s_status_lbl) return;
    lv_label_set_text(s_status_lbl, txt);
    lv_obj_set_style_text_color(s_status_lbl,
                                ok ? UI_COLOR_GREEN : UI_COLOR_RED, 0);
}

/* El Volver del formulario de Wi-Fi no sale de Configuracion: vuelve al menu,
 * que es de donde se entro. */
/* POR QUE NO LLEGAN DATOS (auditoria del 14-oct-2026): desde aqui, "la P4 no
 * esta" y "la P4 habla otro idioma" (firmware mezclado) se veian IGUAL -- un
 * guion y el punto gris -- y no se arreglan igual: una se arregla encendiendo
 * la P4, la otra grabando la placa que se quedo atras. Corto, que es una linea
 * de la pantalla de Ajustes. */
static void enlace_diag_sufijo(char *out, size_t n)
{
    udp_rx_drop_t motivo = UDP_RX_DROP_NO;
    int ver = -1, tam = -1;
    udp_rx_diagnostico(&motivo, &ver, &tam);
    out[0] = 0;
    switch (motivo) {
        case UDP_RX_DROP_TAM:
            snprintf(out, n, " (otro firmware: %dB v%d)", tam, ver);
            break;
        case UDP_RX_DROP_VERSION:
            snprintf(out, n, " (firmware v%d, no v%d)", ver, (int)MINI_PROTO_VERSION);
            break;
        case UDP_RX_DROP_CRC:
            snprintf(out, n, " (llega corrupto)");
            break;
        case UDP_RX_DROP_ORIGEN:
            snprintf(out, n, " (habla otro aparato)");
            break;
        default:
            break;
    }
}

/* Pinta el estado del enlace con la P4. Calla si la pantalla no se ve. */
static void enlace_timer_cb(lv_timer_t *t)
{
    (void)t;
    if (!s_enlace || lv_obj_has_flag(s_enlace, LV_OBJ_FLAG_HIDDEN)) return;
    if (!lv_obj_is_visible(s_enlace)) return;

    /* Y de paso, cuanto lleva en marcha: es lo que deja ver de un vistazo si la
     * pantalla ha vuelto a arrancar desde la ultima vez que se miro (16-oct-2026,
     * en el banco se reiniciaba por el cable USB). Solo se toca el texto cuando
     * cambia de verdad -- cada minuto, no cada segundo. */
    if (s_uptime_lbl) {
        char nuevo[24];
        snprintf(nuevo, sizeof(nuevo), "En marcha: %s", diag_reset_hace_texto());
        if (strcmp(lv_label_get_text(s_uptime_lbl), nuevo) != 0) {
            lv_label_set_text(s_uptime_lbl, nuevo);
        }
    }

    char ssid[33] = {0};
    bool asociado = false;
    int rssi = 0, sin_datos = -1;
    udp_rx_enlace(ssid, sizeof(ssid), &asociado, &rssi, &sin_datos);
    if (!asociado) {
        lv_label_set_text_fmt(s_enlace, "P4: sin red (busco %s)", ssid);
        lv_obj_set_style_text_color(s_enlace, UI_COLOR_ORANGE, 0);
    } else if (sin_datos < 0) {
        /* Asociada pero SIN haber recibido ni un mensaje desde que arranco.
         * Antes esto se pintaba como "sin datos 0s", que se lee justo al reves
         * ("acaba de llegar un dato") cuando significa "nunca ha llegado nada":
         * el usuario lo vio el 2-oct-2026 y no habia quien lo entendiera. */
        char diag[48];
        enlace_diag_sufijo(diag, sizeof(diag));
        lv_label_set_text_fmt(s_enlace, "P4: %d dBm, sin datos todavia%s", rssi, diag);
        lv_obj_set_style_text_color(s_enlace, UI_COLOR_ORANGE, 0);
    } else if (sin_datos > 5) {
        char diag[48];
        enlace_diag_sufijo(diag, sizeof(diag));
        lv_label_set_text_fmt(s_enlace, "P4: %d dBm, sin datos %ds%s", rssi, sin_datos, diag);
        lv_obj_set_style_text_color(s_enlace, UI_COLOR_ORANGE, 0);
    } else {
        lv_label_set_text_fmt(s_enlace, "P4: %d dBm", rssi);
        lv_obj_set_style_text_color(s_enlace, UI_COLOR_GREEN, 0);
    }
}

/* ── ACTUALIZACION POR WI-FI (OTA), 17-oct-2026 ─────────────────────────────
 *
 * Lo pidio el dueño: "¿es posible implementar una actualizacion por OTA en la 5
 * y 3,5?" -> "dale". El trabajo de verdad (preguntar a la P4, descargar 2 MB,
 * comprobar el sha256 y escribir la flash) lo hace net/ota_rx.c en sus propias
 * tareas; la 3,5" necesito ademas el reparto nuevo de particiones (ota_0/ota_1)
 * para tener donde escribir. AQUI solo se pinta su estado y se pide
 * confirmacion: nada de esta pantalla se queda esperando a la red, porque eso
 * congelaria la UI y el watchdog de tareas reiniciaria la placa a mitad de la
 * descarga.
 */

/* Los codigos de "no se pudo empezar", en una linea corta (ver ota_rx.h). */
static const char *ota_inicio_texto(ota_rx_inicio_t r)
{
    switch (r) {
        case OTA_RX_INICIO_OK:        return "";
        case OTA_RX_INICIO_VIAJE:     return "Termina el viaje antes";
        case OTA_RX_INICIO_OCUPADO:   return "Ya hay una actualizacion en marcha";
        case OTA_RX_INICIO_SIN_DATOS: return "Primero comprueba la version";
        case OTA_RX_INICIO_SIN_MEM:   return "Sin memoria para actualizar";
        default:                      return "No se pudo empezar";
    }
}

/* Aceptar el cartel: empieza la descarga. Verde a la derecha, como todas. */
static void ota_aceptar_cb(void *ud)
{
    (void)ud;
    ota_rx_inicio_t r = ota_rx_actualizar();
    if (r != OTA_RX_INICIO_OK && s_ota_lbl) {
        lv_label_set_text(s_ota_lbl, ota_inicio_texto(r));
        lv_obj_set_style_text_color(s_ota_lbl, UI_COLOR_RED, 0);
    }
    /* s_ota_preguntando se queda puesto a proposito: si no arranco (un viaje que
     * empezo entre la pregunta y el "si"), no se vuelve a preguntar lo mismo
     * cada medio segundo. Se limpia al pulsar el boton o al cancelar. */
}

/* Cancelar (el rojo de la izquierda): se olvida la oferta para que el proximo
 * toque del boton vuelva a comprobar de cero. */
static void ota_cancelar_cb(void *ud)
{
    (void)ud;
    ota_rx_olvidar();
    s_ota_preguntando = false;
    if (s_ota_lbl) {
        lv_label_set_text(s_ota_lbl, "");
        lv_obj_set_style_text_color(s_ota_lbl, UI_COLOR_TEXT_DIM, 0);
    }
}

/* El boton "Actualizar pantalla": pregunta a la P4 si hay version nueva. */
static void ota_btn_cb(lv_event_t *e)
{
    (void)e;
    ota_rx_vista_t v;
    ota_rx_estado(&v);
    if (v.estado == OTA_RX_COMPROBANDO || v.estado == OTA_RX_DESCARGANDO ||
        v.estado == OTA_RX_LISTO) {
        return;   /* ya hay algo en marcha: no se lanza otra */
    }
    /* Este toque es "vuelve a empezar": si la comprobacion anterior dejo el
     * cartel ya preguntado, se limpia para que pregunte otra vez si hay nueva. */
    s_ota_preguntando = false;

    ota_rx_inicio_t r = ota_rx_comprobar();
    if (!s_ota_lbl) return;
    if (r != OTA_RX_INICIO_OK) {
        lv_label_set_text(s_ota_lbl, ota_inicio_texto(r));
        lv_obj_set_style_text_color(s_ota_lbl, UI_COLOR_RED, 0);
    } else {
        lv_label_set_text(s_ota_lbl, "Buscando version nueva...");
        lv_obj_set_style_text_color(s_ota_lbl, UI_COLOR_TEXT_SOFT, 0);
    }
}

/* Pinta el estado y pide confirmacion cuando hay version nueva.
 *
 * Corre en la tarea de LVGL (es un lv_timer) y SOLO mira el estado: ota_rx_estado
 * copia y vuelve. El estado de la OTA y el del enlace comparten sitio en el menu:
 * mientras hay algo que contar de la actualizacion se enseña eso y se esconde el
 * enlace, y al reves al terminar. Asi la fila de botones nueva no le quita alto a
 * la tarjeta de la version. */
static void ota_timer_cb(lv_timer_t *t)
{
    (void)t;

    /* ANTES DE NADA: avisar a la OTA automatica de si hay algo delante que no
     * se puede interrumpir. Esto corre en la tarea de LVGL, que es la dueña de
     * los objetos; ota_rx.c lo lee desde su temporizador de 60 s y no puede
     * tocar LVGL. Se hace aunque Ajustes no este delante: el editor y los
     * dialogos viven en lv_layer_top() y pueden estar abiertos en otra
     * pantalla. */
    ota_rx_ui_ocupada(entry_screen_is_open() || confirm_screen_is_open());

    /* Solo con Ajustes DELANTE y en el MENU (no en el formulario de Wi-Fi): si
     * no, el cartel de confirmacion saltaria encima de lo que el usuario este
     * mirando. */
    if (!s_ota_lbl || !s_enlace || !s_raiz || !s_menu) return;
    if (lv_scr_act() != s_raiz) return;
    if (lv_obj_has_flag(s_menu, LV_OBJ_FLAG_HIDDEN)) return;

    ota_rx_vista_t v;
    ota_rx_estado(&v);

    if (v.estado == OTA_RX_QUIETO) {
        lv_obj_add_flag(s_ota_lbl, LV_OBJ_FLAG_HIDDEN);
        lv_obj_clear_flag(s_enlace, LV_OBJ_FLAG_HIDDEN);
        return;
    }
    lv_obj_clear_flag(s_ota_lbl, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(s_enlace, LV_OBJ_FLAG_HIDDEN);

    switch (v.estado) {
        case OTA_RX_COMPROBANDO:
            lv_label_set_text(s_ota_lbl, "Buscando version nueva...");
            lv_obj_set_style_text_color(s_ota_lbl, UI_COLOR_TEXT_SOFT, 0);
            break;

        case OTA_RX_HAY_NUEVA: {
            lv_label_set_text_fmt(s_ota_lbl, "Hay version nueva: %s", v.ofrecida);
            lv_obj_set_style_text_color(s_ota_lbl, UI_COLOR_CYAN, 0);
            if (!s_ota_preguntando && !confirm_screen_is_open()) {
                s_ota_preguntando = true;
                const esp_app_desc_t *app = esp_app_get_description();
                snprintf(s_ota_body, sizeof(s_ota_body),
                         "Instalada: %s   Nueva: %s",
                         (app && app->version[0]) ? app->version : "?", v.ofrecida);
                /* Verde = aceptar (derecha), rojo = cancelar (izquierda): la
                 * norma del dueño, y confirm_screen ya la cumple. */
                confirm_screen_open("Actualizar pantalla?", s_ota_body,
                                    UI_COLOR_ORANGE_HEX, "Si, actualizar", "Cancelar",
                                    ota_aceptar_cb, NULL);
                /* El rojo tiene que OLVIDAR la oferta. Sin esto, al cancelar el
                 * estado seguiria en HAY_NUEVA y el cartel volveria a salir en
                 * el siguiente tic: un bucle imposible de cerrar. */
                confirm_screen_no_accion(ota_cancelar_cb, NULL);
            }
            break;
        }

        case OTA_RX_AL_DIA: {
            const esp_app_desc_t *app = esp_app_get_description();
            lv_label_set_text_fmt(s_ota_lbl, "Estas al dia (%s)",
                                  (app && app->version[0]) ? app->version : "?");
            lv_obj_set_style_text_color(s_ota_lbl, UI_COLOR_GREEN, 0);
            break;
        }

        case OTA_RX_DESCARGANDO:
            /* La automatica avisa de donde viene: el dueño subio el .bin desde
             * el movil y no ha tocado la pantalla, asi que el cartel se lo
             * cuenta sin pedirle que confirme nada. */
            if (v.automatica) {
                lv_label_set_text_fmt(s_ota_lbl,
                    "Actualizacion recibida de la P4: descargando... %d %%", v.pct);
            } else {
                lv_label_set_text_fmt(s_ota_lbl, "Descargando... %d %%", v.pct);
            }
            lv_obj_set_style_text_color(s_ota_lbl, UI_COLOR_YELLOW, 0);
            break;

        case OTA_RX_LISTO:
            lv_label_set_text(s_ota_lbl, "Listo, reiniciando");
            lv_obj_set_style_text_color(s_ota_lbl, UI_COLOR_GREEN, 0);
            break;

        case OTA_RX_ERROR:
            lv_label_set_text(s_ota_lbl,
                              v.motivo[0] ? v.motivo : "No se pudo actualizar");
            lv_obj_set_style_text_color(s_ota_lbl, UI_COLOR_RED, 0);
            break;

        default:
            break;
    }
}

/* (aqui vivia brillo_btn_cb, el boton de brillo y contraste del menu: quitado el
 * 16-oct-2026, como en la pantalla de 5".)
 *
 * El brillo (y el contraste, que en la 3,5" van juntos) se cambian con el DOBLE
 * TOQUE o el TOQUE LARGO en la pantalla de datos -- view_info.c, doble_toque_cb y
 * toque_largo_cb, los dos llaman a brillo_alternar(). El boton existia porque el
 * gesto no se descubre solo, pero en el menu sobraba.
 * OJO: si algun dia se quitan tambien los gestos, aqui se queda sin forma de
 * cambiar el brillo en marcha. */

static void wifi_back_cb(lv_event_t *e)
{
    (void)e;
    mostrar_menu(true);
}

static void wifi_open_cb(lv_event_t *e)
{
    (void)e;
    mostrar_menu(false);
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

    /* Sin SSID no hay red que guardar (ver udp_rx_set_credentials): se dice POR
     * QUE en vez del generico "No he podido guardarlo", que no explica nada al
     * que acaba de borrar el campo sin querer. No se toca nada mas: ni la NVS,
     * ni la referencia de "lo que habia", ni las credenciales del portal. */
    if (ssid[0] == '\0') {
        wifi_estado("Falta el SSID: no lo he guardado", false);
        ESP_LOGW(TAG, "Guardado cancelado: el SSID esta vacio");
        return;
    }

    bool ok_wifi = udp_rx_set_credentials(ssid, pass);

    /* La referencia pasa a ser lo recien guardado: un segundo toque seguido ya
     * no es un cambio y no volvera a preguntar. */
    snprintf(s_ssid_orig, sizeof(s_ssid_orig), "%s", ssid);
    snprintf(s_pass_orig, sizeof(s_pass_orig), "%s", pass);

    bool ok_portal = (save_portal_creds(lv_textarea_get_text(s_http_user_ta),
                                         lv_textarea_get_text(s_http_pass_ta))
                      == ESP_OK);

    if (ok_wifi && ok_portal) {
        wifi_estado("Guardado, reconectando...", true);
        ESP_LOGI(TAG, "Nuevas credenciales guardadas desde Ajustes");
    } else {
        wifi_estado("No he podido guardarlo", false);
        ESP_LOGW(TAG, "Guardado fallido: wifi=%d portal=%d", ok_wifi, ok_portal);
    }
}

/* Guarda SOLO las credenciales del PORTAL. Es la accion del boton rojo
 * ("Cancelar") del cartel "Cambiar de red?" (D7).
 *
 * POR QUE HACE FALTA: si has cambiado la red, el cartel pregunta antes de
 * tocar la Wi-Fi. Si decides cancelar, hasta ahora se salia sin guardar NADA,
 * asi que lo que habias tecleado en Usuario/Clave del portal se perdia sin
 * avisar (la red no se toca, pero el portal si se puede guardar sin riesgo).
 * Este callback guarda esa parte y lo dice en la linea de estado. */
static void guardar_solo_portal(void *ud)
{
    (void)ud;
    if (save_portal_creds(lv_textarea_get_text(s_http_user_ta),
                          lv_textarea_get_text(s_http_pass_ta)) == ESP_OK) {
        wifi_estado("Guardado.", true);
    } else {
        wifi_estado("No he podido guardarlo", false);
    }
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
        guardar_solo_portal(NULL);
        return;
    }

    static char body[64];   /* estatico: el dialogo lo sigue leyendo despues */
    snprintf(body, sizeof(body), "Red:  %s", ssid);
    /* Naranja, el mismo color del titulo de esta pantalla. */
    confirm_screen_open("Cambiar de red?", body, 0xFF9800,
                        "Si, cambiar", "Cancelar", do_guardar, NULL);
    /* El rojo ("Cancelar") no puede tirar lo tecleado del portal: guarda SOLO
     * esa parte y vuelve al formulario. Se engancha DESPUES de open porque
     * cada apertura limpia la accion del boton izquierdo. */
    confirm_screen_no_accion(guardar_solo_portal, NULL);
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
    mostrar_menu(true);

    wifi_estado("", true);
}

void view_ajustes_create(lv_obj_t *parent)
{
    /* Se guarda la pantalla para que el temporizador de la OTA sepa si esta
     * delante (ver ota_timer_cb). */
    s_raiz = parent;

    lv_obj_set_style_bg_color(parent, UI_COLOR_BG, 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_all(parent, 0, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    /* ── El MENU: un boton de Wi-Fi y, en el resto, la version ───────────── */
    s_menu = lv_obj_create(parent);
    lv_obj_set_size(s_menu, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(s_menu, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_menu, 0, 0);
    lv_obj_set_style_pad_all(s_menu, 8, 0);
    lv_obj_set_style_pad_row(s_menu, 8, 0);
    lv_obj_clear_flag(s_menu, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_flex_flow(s_menu, LV_FLEX_FLOW_COLUMN);

    lv_obj_t *mrow = lv_obj_create(s_menu);
    lv_obj_set_size(mrow, lv_pct(100), 34);
    lv_obj_set_style_bg_opa(mrow, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(mrow, 0, 0);
    lv_obj_set_style_pad_all(mrow, 0, 0);
    lv_obj_clear_flag(mrow, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *mback = lv_btn_create(mrow);
    lv_obj_set_size(mback, CAB_BACK_W, 32);
    lv_obj_set_style_bg_color(mback, UI_COLOR_BOTON, 0);
    lv_obj_align(mback, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(mback, back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *mback_lbl = lv_label_create(mback);
    lv_label_set_text(mback_lbl, LV_SYMBOL_LEFT " Volver");
    lv_obj_center(mback_lbl);

    lv_obj_t *mtitle = lv_label_create(mrow);
    lv_label_set_text(mtitle, "CONFIGURACIÓN");
    lv_obj_set_style_text_color(mtitle, UI_COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(mtitle, &lv_font_montserrat_22, 0);
    titulo_cabecera(mtitle);

    /* DOS ACCIONES EN LA MISMA FILA: Wi-Fi y "Actualizar pantalla".
     *
     * La actualizacion por Wi-Fi la pidio el dueño el 17-oct-2026 y vive aqui
     * porque es donde se mira la version instalada. Comparten fila y alto (64
     * px) para no gastar una linea de las pocas que hay en 320 px: dos botones
     * de 228 px con 8 de aire suman los 464 utiles. */
    lv_obj_t *acciones = lv_obj_create(s_menu);
    lv_obj_set_size(acciones, lv_pct(100), 64);
    lv_obj_set_style_bg_opa(acciones, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(acciones, 0, 0);
    lv_obj_set_style_pad_all(acciones, 0, 0);
    lv_obj_clear_flag(acciones, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *wbtn = lv_btn_create(acciones);
    lv_obj_set_size(wbtn, 228, 64);
    lv_obj_set_style_bg_color(wbtn, UI_COLOR_CYAN, 0);
    lv_obj_align(wbtn, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(wbtn, wifi_open_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *wlbl = lv_label_create(wbtn);
    lv_label_set_text(wlbl, LV_SYMBOL_WIFI "   Wi-Fi");
    lv_obj_set_style_text_font(wlbl, &lv_font_montserrat_24, 0);
    lv_obj_center(wlbl);

    lv_obj_t *ubtn = lv_btn_create(acciones);
    lv_obj_set_size(ubtn, 228, 64);
    lv_obj_set_style_bg_color(ubtn, UI_COLOR_CYAN, 0);
    lv_obj_align(ubtn, LV_ALIGN_RIGHT_MID, 0, 0);
    lv_obj_add_event_cb(ubtn, ota_btn_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *ulbl = lv_label_create(ubtn);
    lv_label_set_text(ulbl, "Actualizar pantalla");
    /* Letra 20 y no 24: en 228 px, "Actualizar pantalla" con la 24 se sale. La
     * 20 (Inter, con acentos) entra justa y se sigue leyendo. */
    lv_obj_set_style_text_font(ulbl, &lv_font_montserrat_20, 0);
    lv_obj_center(ulbl);

    /* AQUI ESTABA EL BOTON DE BRILLO Y CONTRASTE. El usuario lo mando quitar el
     * 16-oct-2026 (el mismo dia que en la 5"): el gesto de la pantalla de datos
     * es donde se esta mirando, y en el menu solo hacia ruido. */

    /* Estado del enlace con la P4. Vive AQUI y no en la pantalla de datos
     * (mudado el 30-sep-2026): es lo que se mira cuando algo no va, y en
     * Configuracion no le quita sitio a ningun numero. Lo pinta un temporizador
     * de un segundo que solo trabaja si esta visible. */
    s_enlace = lv_label_create(s_menu);
    lv_obj_set_style_text_font(s_enlace, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_enlace, UI_COLOR_TEXT_DIM, 0);
    lv_label_set_text(s_enlace, "");
    if (!s_enlace_timer) {
        s_enlace_timer = lv_timer_create(enlace_timer_cb, 1000, NULL);
    }

    /* EL ESTADO DE LA ACTUALIZACION. Comparte sitio con el del enlace (ver
     * ota_timer_cb): nace escondido y se enseña solo cuando hay algo que contar,
     * asi que la fila de dos botones no le quita alto a la tarjeta de abajo. */
    s_ota_lbl = lv_label_create(s_menu);
    lv_obj_set_style_text_font(s_ota_lbl, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_ota_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_label_set_text(s_ota_lbl, "");
    lv_obj_set_width(s_ota_lbl, lv_pct(100));
    lv_label_set_long_mode(s_ota_lbl, LV_LABEL_LONG_WRAP);
    lv_obj_add_flag(s_ota_lbl, LV_OBJ_FLAG_HIDDEN);
    /* 500 ms: el progreso se mueve como mucho cada ~40 ms, asi que medio segundo
     * va sobrado. El temporizador no hace nada si Ajustes no esta delante. */
    if (!s_ota_timer) {
        s_ota_timer = lv_timer_create(ota_timer_cb, 500, NULL);
        if (!s_ota_timer) ESP_LOGW(TAG, "sin temporizador de OTA: no se vera el progreso");
    }

    /* Y el resto de la pantalla, la VERSION. GRANDE y centrada -- letra 40 para
     * el numero y 20 para el rotulo y la fecha: sobra sitio y esto se mira de
     * lejos, con el aparato en su soporte y sin acercarse. Que es el
     * primer dato que hace falta cuando algo va raro: sale del tag de git al
     * compilar (esp_app_get_description), no hay ningun numero que mantener a
     * mano. Con la fecha debajo, que distingue dos builds de la misma. */
    lv_obj_t *vcard = lv_obj_create(s_menu);
    lv_obj_set_width(vcard, lv_pct(100));
    lv_obj_set_flex_grow(vcard, 1);
    lv_obj_set_style_bg_color(vcard, UI_COLOR_CARD, 0);
    lv_obj_set_style_bg_opa(vcard, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(vcard, 0, 0);
    lv_obj_set_style_radius(vcard, 10, 0);
    lv_obj_set_style_pad_all(vcard, 6, 0);
    lv_obj_set_style_pad_row(vcard, 2, 0);
    /* CON SCROLL, aunque quepa: sin el, lo que no cabe se recorta en silencio
     * (el usuario lo vio en la 5": informacion perdida arriba y abajo del numero
     * de version). 16-oct-2026. */
    lv_obj_set_scroll_dir(vcard, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(vcard, LV_SCROLLBAR_MODE_AUTO);
    lv_obj_set_flex_flow(vcard, LV_FLEX_FLOW_COLUMN);
    /* START Y NO CENTER EN EL EJE PRINCIPAL (auditoria del 16-oct-2026): con
     * CENTER, lo que sobresale por ARRIBA de un contenedor con scroll queda
     * FUERA del rango alcanzable -- el scroll solo llega de [-scroll_bottom, 0],
     * asi que lo de arriba no se puede ni deslizar ni tocar, y el cartel rojo de
     * dos lineas de diag_reset_resumen() se recortaba unos 9 px. Alineado
     * ARRIBA, la tarjeta empieza en 0 y todo lo que sobra se alcanza bajando. */
    lv_obj_set_flex_align(vcard, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER,
                          LV_FLEX_ALIGN_CENTER);

    const esp_app_desc_t *app = esp_app_get_description();

    lv_obj_t *vrot = lv_label_create(vcard);
    lv_label_set_text(vrot, "Versión instalada");
    lv_obj_set_style_text_color(vrot, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(vrot, &lv_font_montserrat_20, 0);

    s_ver_lbl = lv_label_create(vcard);
    lv_label_set_text(s_ver_lbl, app ? app->version : "?");
    lv_obj_set_style_text_color(s_ver_lbl, UI_COLOR_TEXT, 0);
    lv_obj_set_style_text_font(s_ver_lbl, &lv_font_montserrat_40, 0);

    lv_obj_t *vfecha = lv_label_create(vcard);
    if (app) lv_label_set_text_fmt(vfecha, "%s  %s", app->date, app->time);
    else     lv_label_set_text(vfecha, "");
    lv_obj_set_style_text_color(vfecha, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(vfecha, &lv_font_montserrat_20, 0);

    /* Chivato de reinicio (ver diag_reset.c): aparece SOLO si el arranque
     * anterior no lo provoco el contacto (watchdog, panic, cuelgue). Con el
     * ajetreo normal -- esta pantalla arranca con cada contacto, varias veces
     * al dia -- una tarjeta fija seria puro ruido; y este es el unico momento
     * en que se puede saber, porque al siguiente corte de contacto el motivo se
     * pierde. Una sola etiqueta y en 16 para no crecer la tarjeta: vcard es
     * flex_grow y NO tiene scroll, asi que lo que no quepa se recorta. */
    if (diag_reset_fue_fallo()) {
        lv_obj_t *vrein = lv_label_create(vcard);
        lv_label_set_text(vrein, diag_reset_resumen());
        lv_obj_set_style_text_align(vrein, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_style_text_color(vrein, UI_COLOR_RED, 0);
        lv_obj_set_style_text_font(vrein, &lv_font_montserrat_16, 0);
    } else {
        /* SI EL ARRANQUE NO FUE UN FALLO, SE DICE IGUAL (16-oct-2026, el
         * usuario: "si, en configuracion"). Un BAJON DE TENSION o un RESET POR
         * EL CABLE USB no cuentan como fallo (es_fallo() los deja fuera a
         * proposito: en la autocaravana el arranque del motor hunde la tension),
         * asi que no salian en ninguna pantalla -- y son justo lo que hay que
         * ver cuando la pantalla parece reiniciarse sola. En gris y pequenito:
         * informa sin dar la alarma que da el chivato rojo de arriba. */
        const char *motivo = diag_reset_motivo_arranque();
        if (motivo && motivo[0]) {
            lv_obj_t *vmot = lv_label_create(vcard);
            lv_label_set_text_fmt(vmot, "Arranque: %s", motivo);
            lv_obj_set_style_text_align(vmot, LV_TEXT_ALIGN_CENTER, 0);
            lv_obj_set_style_text_color(vmot, UI_COLOR_TEXT_DIM, 0);
            lv_obj_set_style_text_font(vmot, &lv_font_montserrat_16, 0);
        }
    }

    /* CUANTO LLEVA EN MARCHA, siempre y en gris (16-oct-2026). Se actualiza sola
     * con el temporizador del enlace (1 s, y solo con la pantalla visible). Si
     * pone "En marcha: 2 min" cuando hace un rato ponia "1 h 10 min", es que ha
     * arrancado otra vez: sin PC, sin log y sin tocar el puerto serie. */
    s_uptime_lbl = lv_label_create(vcard);
    lv_label_set_text_fmt(s_uptime_lbl, "En marcha: %s", diag_reset_hace_texto());
    lv_obj_set_style_text_align(s_uptime_lbl, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_set_style_text_color(s_uptime_lbl, UI_COLOR_TEXT_DIM, 0);
    lv_obj_set_style_text_font(s_uptime_lbl, &lv_font_montserrat_16, 0);

    /* ── El formulario del WI-FI ─────────────────────────────────────────── */
    s_wifi = lv_obj_create(parent);
    lv_obj_set_size(s_wifi, lv_pct(100), lv_pct(100));
    lv_obj_set_style_bg_opa(s_wifi, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(s_wifi, 0, 0);
    lv_obj_set_style_pad_all(s_wifi, 8, 0);
    lv_obj_set_style_pad_row(s_wifi, 4, 0);
    lv_obj_set_flex_flow(s_wifi, LV_FLEX_FLOW_COLUMN);
    /* Cuatro campos y dos explicaciones NO caben en 320 px de alto, asi que
     * esta pantalla se desliza. Solo en vertical: en horizontal no hay nada que
     * ver y ademas confundiria con el gesto del carrusel. */
    lv_obj_set_scroll_dir(s_wifi, LV_DIR_VER);
    lv_obj_set_scrollbar_mode(s_wifi, LV_SCROLLBAR_MODE_AUTO);

    lv_obj_t *row = lv_obj_create(s_wifi);
    lv_obj_set_size(row, lv_pct(100), 34);
    lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(row, 0, 0);
    lv_obj_set_style_pad_all(row, 0, 0);
    lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *back = lv_btn_create(row);
    lv_obj_set_size(back, CAB_BACK_W, 32);
    lv_obj_set_style_bg_color(back, UI_COLOR_BOTON, 0);
    lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
    lv_obj_add_event_cb(back, wifi_back_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *back_lbl = lv_label_create(back);
    lv_label_set_text(back_lbl, LV_SYMBOL_LEFT " Volver");
    lv_obj_center(back_lbl);

    lv_obj_t *title = lv_label_create(row);
    lv_label_set_text(title, "AJUSTES WI-FI");
    lv_obj_set_style_text_color(title, UI_COLOR_ORANGE, 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    titulo_cabecera(title);

    /* DOS GRUPOS CON CABECERA, como en la 5" (16-oct-2026): las dos
     * explicaciones que iban sueltas pasan a ser el titulo de cada bloque, y los
     * rotulos se acortan a lo que dice la 5" ("Usuario" y "Clave" en vez de
     * "Usuario del portal" y "Clave del portal"): el grupo ya dice que son los
     * del portal. Asi las dos pantallas se leen igual. */
    make_grupo(s_wifi, "RED DE LA P4 (la del wifi)");

    /* 31 Y 63, no 32 y 64 (16-oct-2026): el campo del driver (wifi_config_t,
     * sta.ssid[32] / sta.password[64]) reserva un byte para el cero final, asi
     * que el SSID mas largo que admite son 31 caracteres y la clave 63. Con el
     * tope en 32/64 se podia guardar un valor que wifi_configure_sta() recortaba
     * en silencio (ssid[31] = 0) y el resultado era un NO_AP_FOUND que parecia
     * "la P4 no emite" -- con un simple ESP_LOGW en el log. El editor a pantalla
     * completa hereda este tope solo (entry_screen_open copia el max_length del
     * campo de destino: ver el comentario de make_field). */
    s_ssid_ta = make_field(s_wifi, "SSID", 31, false);          /* s_ssid[33] en udp_rx.c */
    s_pass_ta = make_field(s_wifi, "Password", 63, true);      /* s_pass[65] en udp_rx.c */

    make_grupo(s_wifi, "PORTAL DE LA P4 (usuario y clave, no los del wifi)");

    s_http_user_ta = make_field(s_wifi, "Usuario", 32, false);   /* hu[33] en view_ajustes_refresh */
    s_http_pass_ta = make_field(s_wifi, "Clave", 64, true);      /* hp[65] en view_ajustes_refresh */

    lv_obj_t *btn = lv_btn_create(s_wifi);
    lv_obj_set_width(btn, lv_pct(100));
    lv_obj_set_style_bg_color(btn, UI_COLOR_GREEN, 0);
    lv_obj_add_event_cb(btn, guardar_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t *btn_lbl = lv_label_create(btn);
    lv_label_set_text(btn_lbl, "Guardar y reconectar");
    lv_obj_center(btn_lbl);

    s_status_lbl = lv_label_create(s_wifi);
    lv_obj_set_style_text_font(s_status_lbl, &lv_font_montserrat_14, 0);
    wifi_estado("", true);

    /* Se entra siempre por el menu. */
    mostrar_menu(true);
}
