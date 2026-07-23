#ifndef GESTOR_RESPALDOS_H
#define GESTOR_RESPALDOS_H

#include <stddef.h>

/* Carpeta donde se almacenan los respaldos y el snapshot incremental */
#define DIR_RESPALDOS "respaldos"

/* Crea un respaldo (completo o incremental) de 'directorio_origen' usando
 * 'tar --listed-incremental'. Devuelve 0 en éxito, -1 en error. */
int crear_respaldo_incremental(const char *directorio_origen);

/* Restaura 'archivo_respaldo' dentro de 'ruta_destino'.
 *
 * ADVERTENCIA: antes de extraer, esta función VACÍA por completo el
 * contenido de 'ruta_destino' (para no mezclar archivos viejos con la
 * versión restaurada). La confirmación de esa acción destructiva es
 * responsabilidad de quien llama a esta función (la interfaz), NO de esta
 * capa de lógica.
 *
 * Devuelve 0 en éxito. Devuelve -1 sin extraer nada si: el respaldo no
 * existe, 'ruta_destino' es una ruta insegura (vacía, "/", o una carpeta
 * crítica del sistema), o si la limpieza previa falla a medias (por
 * ejemplo, por falta de permisos sobre algún archivo). */
int restaurar_version(const char *archivo_respaldo, const char *ruta_destino);

/* Llena 'lista' (arreglo de 'max_elementos' cadenas de 256 bytes) con las
 * rutas de los respaldos disponibles, ordenados por nombre (cronológico,
 * ya que el nombre incluye la marca de tiempo). Devuelve la cantidad hallada. */
int listar_respaldos(char lista[][256], int max_elementos);

#endif