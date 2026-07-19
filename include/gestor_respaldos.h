#ifndef GESTOR_RESPALDOS_H
#define GESTOR_RESPALDOS_H

#include <stddef.h>

/* Carpeta donde se almacenan los respaldos y el snapshot incremental */
#define DIR_RESPALDOS "respaldos"

/* Crea un respaldo (completo o incremental) de 'directorio_origen' usando
 * 'tar --listed-incremental'. Devuelve 0 en éxito, -1 en error. */
int crear_respaldo_incremental(const char *directorio_origen);

/* Restaura un respaldo puntual dentro de "respaldos/restaurado".
 * Devuelve 0 en éxito, -1 en error. */
int restaurar_version(const char *archivo_respaldo);

/* Llena 'lista' (arreglo de 'max_elementos' cadenas de 256 bytes) con las
 * rutas de los respaldos disponibles, ordenados por nombre (cronológico,
 * ya que el nombre incluye la marca de tiempo). Devuelve la cantidad hallada. */
int listar_respaldos(char lista[][256], int max_elementos);

#endif