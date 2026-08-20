/* mini_proto.h - Protocolo UDP-broadcast del 7" (P4+C6) hacia los satelites.
 *
 * MANTENER SINCRONIZADO entre DOS proyectos:
 *   - 7"       : ~/joint/victron/main/net/mini_proto.h
 *   - 35cabina : ~/joint/35cabina/main/net/mini_proto.h (este fichero)
 *
 * El tercero, el mini C6 1.47" (~/joint/victron_mini), esta DESCARTADO desde el
 * 20-ago-2026: la 35cabina lo sustituye. Ya NO condiciona el diseno de este
 * protocolo. Aviso practico: mientras el C6 siga fisicamente en marcha, subir
 * MINI_PROTO_VERSION lo deja mudo (rechaza las versiones que no conoce), asi que
 * conviene retirarlo antes o asumirlo.
 *
 * Cambios en el struct requieren bump de MINI_PROTO_VERSION y recompilar los dos.
 *
 * Transporte: UDP broadcast a 192.168.4.255:MINI_PROTO_UDP_PORT.
 *   (intentamos primero ESP-NOW pero esp_hosted no exporta esa API.)
 * Topología: el satelite se asocia al SoftAP del 7" como cliente STA (DHCP).
 * Cadencia: 1 Hz desde el 7".
 *
 * Para valores "sin dato" usar el sentinel definido por campo.
 */
#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define MINI_PROTO_VERSION   2
#define MINI_PROTO_UDP_PORT  4242
#define MINI_NO_DATA_I16     INT16_MIN
#define MINI_NO_DATA_I32     INT32_MIN
#define MINI_NO_DATA_U8      0xFF

/* Payload broadcast 7" -> mini. Tamaño fijo, sin punteros, packed.
 * Total: 32 bytes (sizeof(struct mini_msg), verificado con el compilador). */
struct __attribute__((packed)) mini_msg {
    uint8_t  version;
    uint8_t  _pad0;

    /* Batería principal (SmartShunt / BMV) */
    int16_t  shunt_soc_deci;
    int16_t  shunt_voltage_centi;
    int32_t  shunt_current_milli;

    /* DC/DC (sólo voltajes, no hay corriente cacheada en el 7"). */
    int16_t  dcdc_v_in_centi;
    int16_t  dcdc_v_out_centi;
    uint8_t  dcdc_state;
    uint8_t  _pad1;

    /* Frigorífico */
    int16_t  frigo_temp_centi;
    uint8_t  frigo_fan_pct;
    uint8_t  _pad2;

    /* Aguas (NE185, 0..3). MINI_NO_DATA_U8 si !fresh. */
    uint8_t  water_clean;
    uint8_t  water_gray;

    /* Canal auxiliar del SmartShunt (mismo campo "aux" que victron_records.h
     * en el 7": crudo, la unidad depende de aux_input). aux_input=MINI_NO_DATA_U8
     * si no hay shunt. */
    uint16_t aux_value_raw;       /* V*100 (aux_input 0/1) o Kelvin*100 (2) */
    uint8_t  aux_input;           /* 0=voltage2(arranque), 1=mid-point, 2=temp */

    /* Exterior (sin sensor aún -> MINI_NO_DATA_I16). */
    int16_t  exterior_temp_centi;
    uint8_t  screensaver;         /* 1 = el 7"(P4) está en salvapantallas. El mini ya no lo usa para
                                    * su brillo (es independiente, ver backlight_toggle_dim en main.c);
                                    * se deja en el protocolo por si se usa para otra cosa. */

    uint32_t crc32;               /* CRC32 sobre los bytes [0 .. crc32) */
};

typedef struct mini_msg mini_msg_t;

#ifdef __cplusplus
}
#endif
