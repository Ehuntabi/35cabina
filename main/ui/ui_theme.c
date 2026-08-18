#include "ui_theme.h"
#include "esp_timer.h"

#define CARD_RADIUS     14
#define CARD_BORDER_W   2
#define CARD_PAD        8
#define PULSE_MS        600
#define SEP_W           60
#define SEP_H           2

lv_color_t ui_theme_role_color(ui_role_t role)
{
    switch (role) {
    case UI_ROLE_BATTERY:  return lv_color_hex(0xFF9800);
    case UI_ROLE_SOLAR:    return lv_color_hex(0x4CD964);
    case UI_ROLE_DCDC:     return lv_color_hex(0x4FC3F7);
    case UI_ROLE_INVERTER: return lv_color_hex(0x9C27B0);
    case UI_ROLE_FRIDGE:   return lv_color_hex(0x66CCFF);
    case UI_ROLE_TEMP_EXT: return lv_color_hex(0xFFD54F);
    case UI_ROLE_NEUTRAL:
    default:               return lv_color_hex(0x666666);
    }
}

lv_color_t ui_theme_color_for_soc(int16_t soc_deci)
{
    if (soc_deci >= 600) return UI_COL_GOOD;
    if (soc_deci >= 300) return UI_COL_WARN;
    return UI_COL_BAD;
}

lv_color_t ui_theme_color_for_fridge_temp(int16_t centi)
{
    if (centi <= -1500) return UI_COL_GOOD;
    if (centi <=  -500) return UI_COL_WARN;
    return UI_COL_BAD;
}

lv_obj_t *ui_theme_card_create(lv_obj_t *parent,
                               ui_role_t role,
                               const char *title,
                               const char *icon_utf8)
{
    lv_color_t border = ui_theme_role_color(role);

    lv_obj_t *card = lv_obj_create(parent);
    lv_obj_set_size(card, LV_PCT(100), LV_SIZE_CONTENT);
    lv_obj_set_style_bg_color(card, UI_COL_CARD_TOP, 0);
    lv_obj_set_style_bg_grad_color(card, UI_COL_CARD_BOT, 0);
    lv_obj_set_style_bg_grad_dir(card, LV_GRAD_DIR_VER, 0);
    lv_obj_set_style_bg_opa(card, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(card, border, 0);
    lv_obj_set_style_border_width(card, CARD_BORDER_W, 0);
    lv_obj_set_style_radius(card, CARD_RADIUS, 0);
    lv_obj_set_style_pad_all(card, CARD_PAD, 0);
    lv_obj_set_style_pad_gap(card, 4, 0);
    lv_obj_set_flex_flow(card, LV_FLEX_FLOW_COLUMN);
    lv_obj_clear_flag(card, LV_OBJ_FLAG_SCROLLABLE);

    if (title) {
        lv_obj_t *header = lv_obj_create(card);
        lv_obj_set_size(header, LV_PCT(100), LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(header, LV_OPA_TRANSP, 0);
        lv_obj_set_style_border_width(header, 0, 0);
        lv_obj_set_style_pad_all(header, 0, 0);
        lv_obj_set_flex_flow(header, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(header, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

        if (icon_utf8) {
            lv_obj_t *ic = lv_label_create(header);
            lv_label_set_text(ic, icon_utf8);
            lv_obj_set_style_text_color(ic, border, 0);
            lv_obj_set_style_text_font(ic, &lv_font_montserrat_22, 0);
        }
        lv_obj_t *t = lv_label_create(header);
        lv_label_set_text(t, title);
        lv_obj_set_style_text_color(t, border, 0);
        lv_obj_set_style_text_font(t, &lv_font_montserrat_22, 0);

        lv_obj_t *sep = lv_obj_create(card);
        lv_obj_set_size(sep, SEP_W, SEP_H);
        lv_obj_set_style_bg_color(sep, border, 0);
        lv_obj_set_style_bg_opa(sep, LV_OPA_COVER, 0);
        lv_obj_set_style_border_width(sep, 0, 0);
        lv_obj_set_style_radius(sep, 1, 0);
        lv_obj_set_style_pad_all(sep, 0, 0);
        lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
    }

    return card;
}

/* --- Pulse animado del borde --- */
static void anim_border_w_cb(void *var, int32_t v)
{
    lv_obj_set_style_border_width((lv_obj_t *)var, v, 0);
}

void ui_theme_pulse_start(lv_obj_t *card)
{
    if (!card) return;
    lv_anim_del(card, anim_border_w_cb);
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, card);
    lv_anim_set_exec_cb(&a, anim_border_w_cb);
    lv_anim_set_values(&a, CARD_BORDER_W, CARD_BORDER_W + 4);
    lv_anim_set_time(&a, PULSE_MS);
    lv_anim_set_playback_time(&a, PULSE_MS);
    lv_anim_set_repeat_count(&a, LV_ANIM_REPEAT_INFINITE);
    lv_anim_start(&a);
}

void ui_theme_pulse_stop(lv_obj_t *card)
{
    if (!card) return;
    lv_anim_del(card, anim_border_w_cb);
    lv_obj_set_style_border_width(card, CARD_BORDER_W, 0);
}

/* --- Indicador conexión BLE Victron --- */
#define DOT_SIZE             10
#define DOT_DEFAULT_STALE_MS 30000   /* 30 s sin paquetes → rojo */

static uint32_t s_stale_after_ms = DOT_DEFAULT_STALE_MS;

typedef struct {
    uint64_t last_fresh_us;
    lv_timer_t *tick_timer;   /* guardado para poder borrarlo al destruir el dot */
} conn_dot_ctx_t;

static void conn_dot_tick_cb(lv_timer_t *t)
{
    lv_obj_t *dot = (lv_obj_t *)t->user_data;
    if (!dot) return;
    conn_dot_ctx_t *ctx = (conn_dot_ctx_t *)lv_obj_get_user_data(dot);
    if (!ctx) return;
    uint64_t now_us = esp_timer_get_time();
    if (ctx->last_fresh_us == 0) {
        lv_obj_set_style_bg_color(dot, lv_color_hex(0x666666), 0);
    } else {
        uint32_t age_ms = (uint32_t)((now_us - ctx->last_fresh_us) / 1000);
        lv_color_t c = (age_ms < s_stale_after_ms) ? UI_COL_GOOD : UI_COL_BAD;
        lv_obj_set_style_bg_color(dot, c, 0);
    }
}

/* Limpieza al destruir el dot: borra el timer y libera el contexto.
 * Evita use-after-free (timer firing tras delete) + leak del ctx. */
static void conn_dot_delete_cb(lv_event_t *e)
{
    lv_obj_t *dot = lv_event_get_target(e);
    conn_dot_ctx_t *ctx = (conn_dot_ctx_t *)lv_obj_get_user_data(dot);
    if (ctx) {
        if (ctx->tick_timer) lv_timer_del(ctx->tick_timer);
        lv_mem_free(ctx);
        lv_obj_set_user_data(dot, NULL);
    }
}

lv_obj_t *ui_theme_conn_dot_create(lv_obj_t *parent)
{
    lv_obj_t *dot = lv_obj_create(parent);
    lv_obj_set_size(dot, DOT_SIZE, DOT_SIZE);
    lv_obj_set_style_bg_color(dot, lv_color_hex(0x666666), 0);
    lv_obj_set_style_bg_opa(dot, LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(dot, 0, 0);
    lv_obj_set_style_radius(dot, DOT_SIZE / 2, 0);
    lv_obj_set_style_pad_all(dot, 0, 0);
    lv_obj_clear_flag(dot, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);

    conn_dot_ctx_t *ctx = lv_mem_alloc(sizeof(*ctx));
    if (!ctx) {
        /* Sin contexto el dot funciona pero queda fijo en gris.
         * Mejor eso que un crash o un timer huérfano. */
        return dot;
    }
    ctx->last_fresh_us = 0;
    ctx->tick_timer = lv_timer_create(conn_dot_tick_cb, 1000, dot);
    lv_obj_set_user_data(dot, ctx);

    /* Hook de destrucción: para el timer y libera memoria. */
    lv_obj_add_event_cb(dot, conn_dot_delete_cb, LV_EVENT_DELETE, NULL);
    return dot;
}

void ui_theme_conn_dot_mark_fresh(lv_obj_t *dot)
{
    if (!dot) return;
    conn_dot_ctx_t *ctx = (conn_dot_ctx_t *)lv_obj_get_user_data(dot);
    if (ctx) ctx->last_fresh_us = esp_timer_get_time();
}

void ui_theme_conn_dot_set_threshold(uint32_t stale_after_ms)
{
    s_stale_after_ms = stale_after_ms;
}
