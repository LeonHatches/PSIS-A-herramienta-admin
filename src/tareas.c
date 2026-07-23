#include "tareas.h"
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <signal.h>

enum {
    COLUMNA_PID = 0,
    COLUMNA_NOMBRE,
    COLUMNA_CPU,
    COLUMNA_MEMORIA,
    NUM_COLUMNAS
};

// ==========================================
// MÓDULO: Listar procesos, CPU y Memoria
// ==========================================
static void listar_procesos(GtkListStore *modelo, const char *filtro) {
    struct dirent *entrada;
    DIR *directorio = opendir("/proc");

    if (directorio == NULL) return;

    gtk_list_store_clear(modelo);

    long tamano_pagina = sysconf(_SC_PAGESIZE);
    long ticks_reloj = sysconf(_SC_CLK_TCK);

    while ((entrada = readdir(directorio)) != NULL) {
        if (isdigit(entrada->d_name[0])) {
            char ruta[512];
            char nombre[256] = "Desconocido";
            char str_cpu[64] = "0 s";
            char str_mem[64] = "0 MB";
            FILE *archivo;

            snprintf(ruta, sizeof(ruta), "/proc/%s/comm", entrada->d_name);
            archivo = fopen(ruta, "r");
            if (archivo != NULL) {
                if (fgets(nombre, sizeof(nombre), archivo) != NULL) {
                    nombre[strcspn(nombre, "\n")] = 0; 
                }
                fclose(archivo);
            }

            if (filtro != NULL && strlen(filtro) > 0) {
                if (strstr(nombre, filtro) == NULL && strstr(entrada->d_name, filtro) == NULL) {
                    continue; 
                }
            }

            snprintf(ruta, sizeof(ruta), "/proc/%s/statm", entrada->d_name);
            archivo = fopen(ruta, "r");
            if (archivo != NULL) {
                long paginas_total, paginas_rss;
                if (fscanf(archivo, "%ld %ld", &paginas_total, &paginas_rss) == 2) {
                    float memoria_mb = (float)(paginas_rss * tamano_pagina) / (1024.0 * 1024.0);
                    snprintf(str_mem, sizeof(str_mem), "%.2f MB", memoria_mb);
                }
                fclose(archivo);
            }

            snprintf(ruta, sizeof(ruta), "/proc/%s/stat", entrada->d_name);
            archivo = fopen(ruta, "r");
            if (archivo != NULL) {
                char linea[1024];
                if (fgets(linea, sizeof(linea), archivo) != NULL) {
                    char *cierre_parentesis = strrchr(linea, ')'); 
                    if (cierre_parentesis != NULL) {
                        unsigned long utime = 0, stime = 0;
                        sscanf(cierre_parentesis + 2, "%*c %*d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &utime, &stime);
                        snprintf(str_cpu, sizeof(str_cpu), "%lu s", (utime + stime) / ticks_reloj);
                    }
                }
                fclose(archivo);
            }

            GtkTreeIter iterador;
            gtk_list_store_append(modelo, &iterador);
            gtk_list_store_set(modelo, &iterador,
                               COLUMNA_PID, entrada->d_name,
                               COLUMNA_NOMBRE, nombre,
                               COLUMNA_CPU, str_cpu,
                               COLUMNA_MEMORIA, str_mem,
                               -1);
        }
    }
    closedir(directorio);
}

// ==========================================
// MÓDULO: Acciones (Finalizar, Suspender, Reanudar)
// ==========================================
static void ejecutar_accion_proceso(GtkWidget *item, gpointer user_data) {
    GtkTreeView *arbol = GTK_TREE_VIEW(user_data);
    GtkTreeSelection *seleccion = gtk_tree_view_get_selection(arbol);
    GtkTreeModel *modelo;
    GtkTreeIter iterador;

    // Si hay una fila seleccionada, extraemos su PID
    if (gtk_tree_selection_get_selected(seleccion, &modelo, &iterador)) {
        gchar *pid_str;
        gtk_tree_model_get(modelo, &iterador, COLUMNA_PID, &pid_str, -1);
        int pid = atoi(pid_str);
        g_free(pid_str);

        // Identificamos qué botón del menú se presionó
        const char *accion = g_object_get_data(G_OBJECT(item), "accion");

        if (strcmp(accion, "matar") == 0) {
            kill(pid, SIGKILL);
        } else if (strcmp(accion, "suspender") == 0) {
            kill(pid, SIGSTOP);
        } else if (strcmp(accion, "reanudar") == 0) {
            kill(pid, SIGCONT);
        }

        // Refrescamos la tabla para ver los cambios
        listar_procesos(GTK_LIST_STORE(modelo), NULL);
    }
}

// ==========================================
// MÓDULO: Menú de clic derecho
// ==========================================
static gboolean mostrar_menu_contextual(GtkWidget *arbol, GdkEventButton *evento, gpointer user_data) {
    if (evento->type == GDK_BUTTON_PRESS && evento->button == 3) {
        
        // Seleccionar automáticamente la fila sobre la que se hizo clic
        GtkTreePath *ruta;
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(arbol), evento->x, evento->y, &ruta, NULL, NULL, NULL)) {
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(arbol), ruta, NULL, FALSE);
            gtk_tree_path_free(ruta);
        }

        // Crear el menú emergente
        GtkWidget *menu = gtk_menu_new();

        GtkWidget *item_matar = gtk_menu_item_new_with_label("Finalizar Proceso");
        g_object_set_data(G_OBJECT(item_matar), "accion", "matar");
        g_signal_connect(item_matar, "activate", G_CALLBACK(ejecutar_accion_proceso), arbol);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_matar);

        GtkWidget *item_suspender = gtk_menu_item_new_with_label("Suspender Proceso");
        g_object_set_data(G_OBJECT(item_suspender), "accion", "suspender");
        g_signal_connect(item_suspender, "activate", G_CALLBACK(ejecutar_accion_proceso), arbol);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_suspender);

        GtkWidget *item_reanudar = gtk_menu_item_new_with_label("Reanudar Proceso");
        g_object_set_data(G_OBJECT(item_reanudar), "accion", "reanudar");
        g_signal_connect(item_reanudar, "activate", G_CALLBACK(ejecutar_accion_proceso), arbol);
        gtk_menu_shell_append(GTK_MENU_SHELL(menu), item_reanudar);

        gtk_widget_show_all(menu);
        gtk_menu_popup_at_pointer(GTK_MENU(menu), (GdkEvent*)evento);

        return TRUE;
    }
    return FALSE;
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
        "Nombre", renderizador, "text", COLUMNA_NOMBRE, NULL);
    gtk_tree_view_append_column(arbol, col_nombre);

    GtkTreeViewColumn *col_cpu = gtk_tree_view_column_new_with_attributes(
        "Tiempo CPU", renderizador, "text", COLUMNA_CPU, NULL);
    gtk_tree_view_append_column(arbol, col_cpu);

    GtkTreeViewColumn *col_mem = gtk_tree_view_column_new_with_attributes(
        "Memoria (RSS)", renderizador, "text", COLUMNA_MEMORIA, NULL);
    gtk_tree_view_append_column(arbol, col_mem);
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

    GtkListStore *modelo = gtk_list_store_new(NUM_COLUMNAS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    GtkWidget *buscador = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(buscador), "Buscar por nombre o PID...");
    g_signal_connect(buscador, "changed", G_CALLBACK(buscar_por_nombre), modelo);
    gtk_box_pack_start(GTK_BOX(caja), buscador, FALSE, FALSE, 0);

    GtkWidget *arbol = gtk_tree_view_new_with_model(GTK_TREE_MODEL(modelo));
    g_object_unref(modelo); 

    // Conectar el evento de clic derecho al árbol
    g_signal_connect(arbol, "button-press-event", G_CALLBACK(mostrar_menu_contextual), NULL);

    configurar_columnas(GTK_TREE_VIEW(arbol));
    listar_procesos(modelo, NULL); 

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_container_add(GTK_CONTAINER(scroll), arbol);

    gtk_box_pack_start(GTK_BOX(caja), scroll, TRUE, TRUE, 0);

    return caja;
}