/* view_inclinacion.c - Placeholder (Fase 2). Sensor real en Fase 3. */
#include "view_inclinacion.h"

void view_inclinacion_create(lv_obj_t *parent)
{
    lv_obj_set_style_bg_color(parent, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(parent, LV_OPA_COVER, 0);
    lv_obj_clear_flag(parent, LV_OBJ_FLAG_SCROLLABLE);

    lv_obj_t *title = lv_label_create(parent);
    lv_label_set_text(title, "INCLINACION");
    lv_obj_set_style_text_color(title, lv_color_hex(0xAB47BC), 0);
    lv_obj_set_style_text_font(title, &lv_font_montserrat_22, 0);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 20);

    lv_obj_t *msg = lv_label_create(parent);
    lv_label_set_text(msg, "Sensor ADXL345 pendiente\n(Fase 3)");
    lv_obj_set_style_text_color(msg, lv_color_hex(0x888888), 0);
    lv_obj_set_style_text_font(msg, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_align(msg, LV_TEXT_ALIGN_CENTER, 0);
    lv_obj_center(msg);
}
