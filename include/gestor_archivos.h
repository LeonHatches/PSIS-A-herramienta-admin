#ifndef GESTOR_ARCHIVOS_H
#define GESTOR_ARCHIVOS_H

#include <gtk/gtk.h>
#include <stddef.h>

/* Índices de columnas del modelo (GtkListStore) usado por la tabla de archivos.
 * COL_RUTA es una columna oculta que guarda la ruta absoluta de cada fila,
 * necesaria para navegar y para las operaciones de copiar/mover/eliminar. */
enum {
    COL_NOMBRE = 0,
    COL_TAMANO,
    COL_TIPO,
    COL_RUTA,
    NUM_COLS_ARCHIVOS
};

/* Lee el contenido real de 'ruta' desde el disco y llena 'modelo'.
 * Devuelve la cantidad de elementos listados, o -1 si la ruta no pudo abrirse. */
int cargar_archivos_reales(GtkListStore *modelo, const char *ruta);

/* Da formato legible a un tamaño en bytes (ej. "1.4 MB"). */
void formatear_tamano(long bytes, char *buffer, size_t buffer_size);

/* Operaciones nativas sobre el sistema de archivos.
 * Soportan tanto archivos individuales como carpetas (de forma recursiva).
 * Devuelven 0 en éxito y distinto de 0 en error. */
int copiar_elemento(const char *origen, const char *destino);
int mover_elemento(const char *origen, const char *destino);
int eliminar_elemento(const char *ruta);

#endif