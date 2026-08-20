/* nav.c - Carrusel de 3 pantallas con gesto horizontal + pantalla de
 * Ajustes aparte (se abre/cierra por boton, no forma parte del gesto). */
#include "nav.h"
#include "view_info.h"
#include "view_registro.h"
#include "view_inclinacion.h"
#include "view_ajustes.h"
#include "lvgl.h"

#define NAV_COUNT       3
#define NAV_INCLINACION 0   /* izquierda */
#define NAV_INFO        1   /* centro (arranque) */
#define NAV_REGISTRO    2   /* derecha */

#define NAV_ANIM_MS     220

static lv_obj_t *s_screens[NAV_COUNT];
static lv_obj_t *s_ajustes_screen;
static uint8_t   s_current = NAV_INFO;

static void gesture_cb(lv_event_t *e)
{
    (void)e;
    lv_indev_t *indev = lv_indev_get_act();
    if (!indev) return;
    lv_dir_t dir = lv_indev_get_gesture_dir(indev);

    int next = s_current;
    lv_scr_load_anim_t anim;
    if (dir == LV_DIR_LEFT && s_current < NAV_COUNT - 1) {
        next = s_current + 1;
        anim = LV_SCR_LOAD_ANIM_MOVE_LEFT;
    } else if (dir == LV_DIR_RIGHT && s_current > 0) {
        next = s_current - 1;
        anim = LV_SCR_LOAD_ANIM_MOVE_RIGHT;
    } else {
        return;
    }
    /* Al abandonar la pagina de registros se vuelve a su menu de iconos: si no,
     * al regresar te encontrabas el formulario abierto donde lo dejaste, en vez
     * del menu. Se pierde lo tecleado a medias, que es lo esperado -- te has
     * ido de la pantalla. */
    if (s_current == NAV_REGISTRO) view_registro_reset();

    s_current = (uint8_t)next;
    lv_scr_load_anim(s_screens[s_current], anim, NAV_ANIM_MS, 0, false);
}

void nav_init(void)
{
    for (int i = 0; i < NAV_COUNT; i++) {
        s_screens[i] = lv_obj_create(NULL);
        lv_obj_add_event_cb(s_screens[i], gesture_cb, LV_EVENT_GESTURE, NULL);
    }

    view_inclinacion_create(s_screens[NAV_INCLINACION]);
    view_info_create(s_screens[NAV_INFO]);
    view_registro_create(s_screens[NAV_REGISTRO]);

    s_ajustes_screen = lv_obj_create(NULL);
    view_ajustes_create(s_ajustes_screen);

    lv_scr_load(s_screens[NAV_INFO]);
}

void nav_open_ajustes(void)
{
    view_ajustes_refresh();
    lv_scr_load_anim(s_ajustes_screen, LV_SCR_LOAD_ANIM_MOVE_TOP, NAV_ANIM_MS, 0, false);
}

void nav_close_ajustes(void)
{
    s_current = NAV_INFO;
    lv_scr_load_anim(s_screens[NAV_INFO], LV_SCR_LOAD_ANIM_MOVE_BOTTOM, NAV_ANIM_MS, 0, false);
}
