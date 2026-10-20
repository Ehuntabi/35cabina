/* brillo.c - Brillo en dos niveles, guardado en NVS.
 *
 * La persistencia NO se hace aqui a mano: se usa config_storage, que es donde
 * guarda sus cosas todo el resto del proyecto (salida, inclinacion, wifi,
 * viaje, parada). load_brightness()/save_brightness() ya existian desde el fork
 * anterior y no las llamaba nadie.
 */
#include "brillo.h"

#include "config_storage.h"
#include "display.h"   /* bsp_display_brightness_set() */
#include "esp_log.h"

static const char *TAG = "brillo";

static uint8_t s_nivel = BRILLO_ALTO;

/* ARRANCA SIEMPRE AL MAXIMO, y el valor guardado NO se aplica al encender.
 *
 * POR QUE (regla que ya seguia la 5" desde el 9-oct-2026, adoptada aqui el
 * 16-oct-2026): un arranque al 25 % con el tema oscuro de esta interfaz se ve
 * como una pantalla APAGADA -- es el fallo que el README avisa de mirar
 * ("La pantalla esta negra -> mirar el brillo antes que nada"). Y como el valor
 * se recuerda, un arranque a oscuras se quedaba grabado y volvia a pasar en el
 * siguiente encendido: la pantalla se apagaba sola y no habia forma de salir sin
 * saber donde tocar.
 *
 * LA REGLA: la luz NUNCA puede depender de lo que quedo guardado. Al encender,
 * 100 %. Si te pasas bajando al 25 % y no ves nada, se desenchufa y vuelve --
 * siempre hay una salida. Lo guardado se sigue leyendo para dejarlo en el log
 * (si el usuario dice "arranco apagada", el log lo dice), pero no se aplica.
 *
 * OJO: la clave de NVS es la MISMA que la de la 5", y alli el valor es un paso
 * de su escala (85/100) mientras que aqui es el porcentaje (30/100). Da igual
 * mientras no se aplique al arrancar -- que es justo lo que se ha quitado --,
 * pero es la razon de no volver a "leer y aplicar". */
void brillo_init(void)
{
    /* Se sigue leyendo para dejar constancia en el log de lo que habia
     * guardado. Ya no se reescribe si no vale (el 5 de fabrica del fork viejo,
     * o el 85 que dejase una 5"): lo guardado ya no manda. */
    uint8_t guardado = 0;
    if (load_brightness(&guardado) != ESP_OK) guardado = 0;

    s_nivel = BRILLO_ALTO;   /* el maximo, mire lo que mire la NVS */

    ESP_LOGI(TAG, "Brillo inicial %u%% (en NVS habia %u%%, no se aplica)",
             (unsigned)s_nivel, (unsigned)guardado);
    bsp_display_brightness_set(s_nivel);
}

uint8_t brillo_nivel(void) { return s_nivel; }

uint8_t brillo_alternar(void)
{
    s_nivel = (s_nivel == BRILLO_ALTO) ? BRILLO_BAJO : BRILLO_ALTO;
    bsp_display_brightness_set(s_nivel);

    /* Se escribe solo al cambiarlo, no periodicamente: son dos toques de vez en
     * cuando, no hay riesgo de desgastar la flash. */
    if (save_brightness(s_nivel) != ESP_OK) {
        ESP_LOGW(TAG, "No se pudo guardar el brillo");
    }
    ESP_LOGI(TAG, "Brillo -> %u%%", s_nivel);
    return s_nivel;
}
