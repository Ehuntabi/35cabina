cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Version del firmware (Ajustes -> Acerca de), con la MISMA regla que la P4
# (13-sep-2026, decision del usuario; auditado el 15-sep-2026 porque la cabina
# la incumplia): la version NUNCA lleva sufijos (ni "-dirty" ni "-N-g<hash>").
# Lo que hay por encima del ultimo tag ES la version siguiente, +0.1: el arbol
# justo en el tag muestra esa version, y con trabajo encima muestra la
# siguiente, sin que aparezca "-dirty" por ningun lado. El +0.1 es solo el
# valor del trabajo en curso: la version de verdad la fija el tag que crea
# release.sh, y en cuanto existe el tag manda el tag.
#
# El patron es "v*.*" y NO "v*.*.*": desde el 24-ago-2026 las versiones llevan
# DOS numeros; el comodin de git abarca tambien el punto, asi que encuentra
# igual las nuevas que las viejas de tres, y las sdkconfig-known-good-* quedan
# fuera por no empezar por v.
#
# Se calcula al CONFIGURAR, asi que se congelaria: por eso el
# CMAKE_CONFIGURE_DEPENDS de abajo (al cambiar el git, se recalcula). Ojo: un
# fichero tocado y SIN anadir no lo dispara; da igual porque ya no se usa el
# sufijo -dirty.
execute_process(
    COMMAND git describe --always --tags --dirty --match "v*.*"
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE VER_DESCRITA
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(VER_DESCRITA MATCHES "^v([0-9]+)\.([0-9]+)(\.[0-9]+)?$")
    set(PROJECT_VER "${VER_DESCRITA}")
elseif(VER_DESCRITA MATCHES "^v([0-9]+)\.([0-9]+)-")
    math(EXPR VER_MINOR_SIGUIENTE "1 + ${CMAKE_MATCH_2}")
    set(PROJECT_VER "v${CMAKE_MATCH_1}.${VER_MINOR_SIGUIENTE}")
else()
    set(PROJECT_VER "v?")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_LIST_DIR}/.git/HEAD"
             "${CMAKE_CURRENT_LIST_DIR}/.git/index")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(35cabina)
