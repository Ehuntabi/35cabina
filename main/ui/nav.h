/* nav.h - Carrusel de 3 pantallas con gesto horizontal, mas la pantalla
 * de Ajustes (fuera del carrusel, se abre/cierra con un icono, no gesto).
 *
 * Centro: info agrupada. Derecha (swipe izda): menu de registros.
 * Izquierda (swipe dcha): inclinacion.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Crea las 3 pantallas del carrusel + la de Ajustes, las llena y carga
 * la de info como pantalla activa inicial. */
void nav_init(void);

/* Abre la pantalla de Ajustes (Wi-Fi) por encima del carrusel. */
void nav_open_ajustes(void);

/* Vuelve del carrusel a la pantalla de info. */
void nav_close_ajustes(void);

#ifdef __cplusplus
}
#endif
