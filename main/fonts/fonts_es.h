/* Fuentes Inter (Regular y Bold) con rango ASCII + acentos espanoles + n + ¿¡° y
 * los 33 iconos FontAwesome que ya usa la P4. Generadas con lv_font_conv y la
 * MISMA receta que la P4: el comando exacto va escrito en la cabecera de cada
 * .c generado.
 *
 * POR QUE (16-oct-2026): las cabinas usaban las Montserrat del SDK, que NO
 * traen acentos ni la n con virgulilla -- de ahi que todos los rotulos fueran
 * en ASCII ("Configuracion") y que el destino del viaje solo admitiera letras
 * sin tilde. Con estas, la UI puede decir "Configuracion" bien escrita.
 *
 * LOS NOMBRES VIEJOS SE MANTIENEN COMO ALIAS, igual que en la P4 (ver su
 * docs/GUIA_ESTILO.md): asi no hay que tocar las ~100 llamadas de la UI y el
 * cambio de familia vive en este fichero. Los alias son NOMBRES DE SIMBOLO,
 * nunca "(&fuente)": en la UI se escriben como &lv_font_montserrat_20.
 *
 * TAMANOS: los mismos px que se renderizaban antes (medidos sobre el codigo):
 * (aqui van los que usa ESTA placa). Lo que cambia es la familia, no el tamano.
 */
#pragma once

#include "lvgl.h"

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_font_t lv_font_inter_14_es;
extern const lv_font_t lv_font_inter_16_es;
extern const lv_font_t lv_font_inter_20_es;
extern const lv_font_t lv_font_inter_22_es;
extern const lv_font_t lv_font_inter_24_es;
extern const lv_font_t lv_font_inter_32_es;
extern const lv_font_t lv_font_inter_40_es;
extern const lv_font_t lv_font_inter_48_es;
extern const lv_font_t lv_font_inter_bold_20_es;
extern const lv_font_t lv_font_inter_bold_32_es;

/* ── Alias de los nombres que ya usaba la UI ───────────────────────────────── */
#define lv_font_montserrat_14 lv_font_inter_14_es
#define lv_font_montserrat_16 lv_font_inter_16_es
#define lv_font_montserrat_20 lv_font_inter_20_es
#define lv_font_montserrat_22 lv_font_inter_22_es
#define lv_font_montserrat_24 lv_font_inter_24_es
#define lv_font_montserrat_32 lv_font_inter_32_es
#define lv_font_montserrat_40 lv_font_inter_40_es
#define lv_font_montserrat_48 lv_font_inter_48_es
#define lv_font_montserrat_bold_20 lv_font_inter_bold_20_es
#define lv_font_montserrat_bold_32 lv_font_inter_bold_32_es

#ifdef __cplusplus
}
#endif
