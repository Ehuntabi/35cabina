/* nav.h - Carrusel de 3 pantallas con gesto horizontal (Fase 2).
 *
 * Centro: info agrupada. Derecha (swipe izda): repostajes/bombona.
 * Izquierda (swipe dcha): inclinacion.
 */
#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* Crea las 3 pantallas, las llena (view_info/view_repostaje/view_inclinacion)
 * y carga la de info como pantalla activa inicial. */
void nav_init(void);

#ifdef __cplusplus
}
#endif
