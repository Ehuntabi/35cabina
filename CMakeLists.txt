cmake_minimum_required(VERSION 3.16)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Version del firmware (Ajustes -> Acerca de). Se fija a mano por dos motivos,
# los dos vistos en la practica:
#
#  1. ESP-IDF hace `git describe --tags` SIN filtro, asi que cualquier etiqueta
#     que no sea de version (las `sdkconfig-known-good-*` de los proyectos
#     hermanos) se colaria en el About. Con --match solo cuentan las v*.*.*
#  2. Lo calcula al CONFIGURAR, no al compilar, asi que se congela: compilas y
#     grabas, y el About sigue enseñando la version de hace veinte commits.
#     Miente con autoridad -- creerias tener instalada la publicada (24-ago-2026:
#     la P4 decia v1.10.0 llevando once commits por encima de esa etiqueta). Por
#     eso el CMAKE_CONFIGURE_DEPENDS de abajo: al cambiar el git, se recalcula.
#     Ojo: un fichero tocado y SIN anadir no lo dispara, asi que el sufijo
#     `-dirty` puede tardar en aparecer.
execute_process(
    COMMAND git describe --always --tags --dirty --match "v*.*.*"
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    OUTPUT_VARIABLE PROJECT_VER
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT PROJECT_VER)
    set(PROJECT_VER "v?")
endif()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
             "${CMAKE_CURRENT_LIST_DIR}/.git/HEAD"
             "${CMAKE_CURRENT_LIST_DIR}/.git/index")

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(35cabina)
