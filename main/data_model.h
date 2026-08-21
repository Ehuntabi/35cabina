#ifndef DATA_MODEL_H
#define DATA_MODEL_H

#include <stdbool.h>
#include <stdint.h>

/* Modelo compartido entre el receptor UDP (net/udp_rx.c) y la UI
 * (ui/view_info.c). Cada bloque tiene flag has_data para saber si pintar
 * valores o "--".
 *
 * Portado de ~/joint/victron_mini/main/data_model.h, con dos añadidos que
 * el mini no usa pero mini_msg_t ya trae: DC/DC y ventilador del frigo.
 * A diferencia del mini, el acceso a g_data está protegido por un spinlock
 * (data_model_get(): la escritura ocurre en rx_task, la lectura en el
 * timer de LVGL — dos tareas distintas, sin eso es una carrera de datos). */

typedef struct {
    bool     has_data;
    uint32_t last_update_ms;   /* uptime en ms cuando llegó el dato */

    /* SmartShunt / BMV */
    int16_t  shunt_soc_deci;       /* SOC * 10  (%)   ej: 782 = 78.2 % */
    int16_t  shunt_voltage_centi;  /* V * 100         ej: 1342 = 13.42 V */
    int32_t  shunt_current_milli;  /* A * 1000  signo */
    int32_t  shunt_power_w;

    /* Canal auxiliar del SmartShunt = bateria de arranque/motor (NO es la
     * bateria de casa). Crudo, la unidad depende de aux_input. */
    uint16_t aux_value_raw;        /* V*100 (aux_input 0/1) o Kelvin*100 (2) */
    uint8_t  aux_input;            /* 0=voltage2(arranque), 1=mid-point, 2=temp */
    bool     aux_has_data;

    /* DC/DC (Orion / cargador). Solo voltajes, sin corriente cacheada en
     * la P4. device_state: ver VIC_STATE_* en victron_records.h del
     * proyecto P4 (0=Off, 3=Bulk, 4=Absorption, 5=Float, ...). */
    int16_t  dcdc_v_in_centi;
    int16_t  dcdc_v_out_centi;
    uint8_t  dcdc_state;
    bool     dcdc_has_data;

    /* Temperaturas DS18B20 + ventilador del frigo */
    int16_t  frigo_temp_centi;     /* °C * 100 */
    uint8_t  frigo_fan_pct;
    int16_t  exterior_temp_centi;
    bool     frigo_has_data;
    bool     exterior_has_data;

    /* Aguas (NE185 de la P4). Niveles 0..3. */
    uint8_t  water_clean;          /* limpia */
    uint8_t  water_gray;           /* grises */
    bool     water_has_data;

    /* Reloj de la P4: segundos desde 1970 ya desplazados a SU hora local, o 0
     * si aun no ha dicho la hora. Es el UNICO reloj que tiene esta pantalla,
     * que no lleva RTC y se apaga con el contacto. Lo usa la parada abierta
     * para contar noches (dividiendo entre 86400 sale el dia) y periodos de
     * 24 h (restando). Ver mini_proto.h. */
    uint32_t epoch_local;
} mini_data_t;

void data_model_init(void);

/* Llamado desde rx_task (net/udp_rx.c) cuando llega un mini_msg_t válido. */
struct mini_msg;  /* forward declare para evitar incluir mini_proto.h aquí */
void data_model_update_from_msg(const struct mini_msg *msg);

/* Copia protegida por lock para que la UI (u otro consumidor) lea un
 * snapshot consistente sin carrera con rx_task. */
void data_model_get(mini_data_t *out);

#endif
