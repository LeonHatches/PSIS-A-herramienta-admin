#ifndef ARCHIVOS_H
#define ARCHIVOS_H

#include <gtk/gtk.h>

/* Construye y devuelve el contenedor raíz del módulo "Shell de Archivos".
 * Internamente gestiona su propio estado (ver ContextoArchivos en archivos.c),
 * por lo que main.c solo necesita llamar a esta función. */
GtkWidget* crear_pantalla_archivos();

#endif