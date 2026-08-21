#include "data_model.h"
#include "net/mini_proto.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include <string.h>

static mini_data_t s_data;
static portMUX_TYPE s_data_mux = portMUX_INITIALIZER_UNLOCKED;

void data_model_init(void) {
    portENTER_CRITICAL(&s_data_mux);
    memset(&s_data, 0, sizeof(s_data));
    portEXIT_CRITICAL(&s_data_mux);

    /* Sin datos demo: arrancamos con has_data=false (todo lo puso el memset)
     * -> la UI muestra "--" en todas las cards hasta que llegue el primer
     * mini_msg real por UDP desde la P4. */
}

void data_model_update_from_msg(const struct mini_msg *msg)
{
    if (!msg || msg->version != MINI_PROTO_VERSION) return;

    /* Construir el snapshot completo en una copia local y solo entrar en
     * la seccion critica para el swap final: la seccion critica queda
     * minima y no bloquea al lector (LVGL) mas de lo estrictamente
     * necesario. */
    mini_data_t tmp;
    portENTER_CRITICAL(&s_data_mux);
    tmp = s_data;
    portEXIT_CRITICAL(&s_data_mux);

    uint32_t now = (uint32_t)(esp_timer_get_time() / 1000);

    /* Shunt — siempre presente, solo se invalida si SoC viene como NO_DATA. */
    if (msg->shunt_soc_deci != MINI_NO_DATA_I16) {
        tmp.has_data            = true;
        tmp.shunt_soc_deci      = msg->shunt_soc_deci;
        tmp.shunt_voltage_centi = msg->shunt_voltage_centi;
        tmp.shunt_current_milli = msg->shunt_current_milli;
        /* P = V * I -> centi V * milli A / 100000 = W, signo conservado. */
        tmp.shunt_power_w = (int32_t)((int64_t)msg->shunt_voltage_centi *
                                       msg->shunt_current_milli / 100000);
    }

    /* Aux del SmartShunt = bateria de arranque/motor. */
    if (msg->aux_input != MINI_NO_DATA_U8) {
        tmp.aux_value_raw = msg->aux_value_raw;
        tmp.aux_input     = msg->aux_input;
        tmp.aux_has_data  = true;
    } else {
        tmp.aux_has_data  = false;
    }

    /* DC/DC — "sin dato" si ambos voltajes vienen NO_DATA. */
    if (msg->dcdc_v_in_centi != MINI_NO_DATA_I16 ||
        msg->dcdc_v_out_centi != MINI_NO_DATA_I16) {
        tmp.dcdc_v_in_centi  = msg->dcdc_v_in_centi;
        tmp.dcdc_v_out_centi = msg->dcdc_v_out_centi;
        tmp.dcdc_state       = msg->dcdc_state;
        tmp.dcdc_has_data    = true;
    } else {
        tmp.dcdc_has_data    = false;
    }

    /* Frigo + ventilador */
    if (msg->frigo_temp_centi != MINI_NO_DATA_I16) {
        tmp.frigo_temp_centi = msg->frigo_temp_centi;
        tmp.frigo_fan_pct    = msg->frigo_fan_pct;
        tmp.frigo_has_data   = true;
    } else {
        tmp.frigo_has_data   = false;
    }

    /* Aguas */
    if (msg->water_clean != MINI_NO_DATA_U8 && msg->water_gray != MINI_NO_DATA_U8) {
        tmp.water_clean    = msg->water_clean;
        tmp.water_gray     = msg->water_gray;
        tmp.water_has_data = true;
    } else {
        tmp.water_has_data = false;
    }

    /* Exterior */
    if (msg->exterior_temp_centi != MINI_NO_DATA_I16) {
        tmp.exterior_temp_centi = msg->exterior_temp_centi;
        tmp.exterior_has_data   = true;
    } else {
        tmp.exterior_has_data   = false;
    }

    /* Dia de calendario. Se copia tal cual, incluido el 0 = "la P4 aun no tiene
     * hora buena": el que lo use ya distingue. */
    tmp.fecha_dias = msg->fecha_dias;

    tmp.last_update_ms = now;

    portENTER_CRITICAL(&s_data_mux);
    s_data = tmp;
    portEXIT_CRITICAL(&s_data_mux);
}

void data_model_get(mini_data_t *out)
{
    if (!out) return;
    portENTER_CRITICAL(&s_data_mux);
    *out = s_data;
    portEXIT_CRITICAL(&s_data_mux);
}
