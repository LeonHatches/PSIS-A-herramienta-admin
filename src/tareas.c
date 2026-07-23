#include "tareas.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

enum {
    COLUMNA_PID = 0,
    COLUMNA_NOMBRE,
    NUM_COLUMNAS
};

// ==========================================
// MÓDULO: Listar procesos
// ==========================================
static void listar_procesos(GtkListStore *modelo, const char *filtro) {
    struct dirent *entrada;
    DIR *directorio = opendir("/proc");

    if (directorio == NULL) return;

    gtk_list_store_clear(modelo);

    while ((entrada = readdir(directorio)) != NULL) {
        if (isdigit(entrada->d_name[0])) {
            char ruta[512];
            char nombre[256] = "Desconocido";
            FILE *archivo;

            snprintf(ruta, sizeof(ruta), "/proc/%s/comm", entrada->d_name);
            archivo = fopen(ruta, "r");
            
            if (archivo != NULL) {
                if (fgets(nombre, sizeof(nombre), archivo) != NULL) {
                    nombre[strcspn(nombre, "\n")] = 0; 
                }
                fclose(archivo);
            }

            // Filtrado de búsqueda
            if (filtro != NULL && strlen(filtro) > 0) {
                if (strstr(nombre, filtro) == NULL && strstr(entrada->d_name, filtro) == NULL) {
                    continue; 
                }
            }

            GtkTreeIter iterador;
            gtk_list_store_append(modelo, &iterador);
            gtk_list_store_set(modelo, &iterador,
                               COLUMNA_PID, entrada->d_name,
                               COLUMNA_NOMBRE, nombre,
                               -1);
        }
    }
    closedir(directorio);
}

// ==========================================
// MÓDULO: Buscar por nombre
// ==========================================
static void buscar_por_nombre(GtkEntry *buscador, gpointer user_data) {
    GtkListStore *modelo = GTK_LIST_STORE(user_data);
    const char *texto = gtk_entry_get_text(buscador);
    
    listar_procesos(modelo, texto);
}

// ==========================================
// CONFIGURACIÓN VISUAL
// ==========================================
static void configurar_columnas(GtkTreeView *arbol) {
    GtkCellRenderer *renderizador = gtk_cell_renderer_text_new();
    
    GtkTreeViewColumn *col_pid = gtk_tree_view_column_new_with_attributes(
        "PID", renderizador, "text", COLUMNA_PID, NULL);
    gtk_tree_view_append_column(arbol, col_pid);

    GtkTreeViewColumn *col_nombre = gtk_tree_view_column_new_with_attributes(
        "Nombre del Proceso", renderizador, "text", COLUMNA_NOMBRE, NULL);
    gtk_tree_view_append_column(arbol, col_nombre);
}

// ==========================================
// ENSAMBLAJE PRINCIPAL
// ==========================================
GtkWidget* crear_pantalla_tareas() {
    GtkWidget *caja = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(caja), "tarjeta");

    GtkWidget *titulo = gtk_label_new("Administrador de Tareas");
    gtk_style_context_add_class(gtk_widget_get_style_context(titulo), "titulo");
    gtk_widget_set_halign(titulo, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(caja), titulo, FALSE, FALSE, 0);

    GtkListStore *modelo = gtk_list_store_new(NUM_COLUMNAS, G_TYPE_STRING, G_TYPE_STRING);

    // Caja de búsqueda (Input)
    GtkWidget *buscador = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(buscador), "Buscar por nombre o PID...");
    g_signal_connect(buscador, "changed", G_CALLBACK(buscar_por_nombre), modelo);
    gtk_box_pack_start(GTK_BOX(caja), buscador, FALSE, FALSE, 0);

    GtkWidget *arbol = gtk_tree_view_new_with_model(GTK_TREE_MODEL(modelo));
    g_object_unref(modelo); 

    configurar_columnas(GTK_TREE_VIEW(arbol));
    listar_procesos(modelo, NULL);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_container_add(GTK_CONTAINER(scroll), arbol);

    gtk_box_pack_start(GTK_BOX(caja), scroll, TRUE, TRUE, 0);

    return caja;
}