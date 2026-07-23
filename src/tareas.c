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

typedef struct {
    int pid;
    int ppid;
    char nombre[256];
    char cpu[64];
    char mem[64];
    int agregado_al_arbol;
} Proceso;

// ==========================================
// MÓDULO: Árbol de procesos (Recursividad)
// ==========================================
static void construir_arbol_recursivo(GtkTreeStore *modelo, GtkTreeIter *padre, int ppid_buscado, Proceso *procesos, int total) {
    for (int i = 0; i < total; i++) {
        if (procesos[i].ppid == ppid_buscado && !procesos[i].agregado_al_arbol) {
            procesos[i].agregado_al_arbol = 1; 
            
            GtkTreeIter iter;
            gtk_tree_store_append(modelo, &iter, padre);
            
            char pid_str[16];
            snprintf(pid_str, sizeof(pid_str), "%d", procesos[i].pid);
            
            gtk_tree_store_set(modelo, &iter,
                               COLUMNA_PID, pid_str,
                               COLUMNA_NOMBRE, procesos[i].nombre,
                               COLUMNA_CPU, procesos[i].cpu,
                               COLUMNA_MEMORIA, procesos[i].mem,
                               -1);
            
            construir_arbol_recursivo(modelo, &iter, procesos[i].pid, procesos, total);
        }
    }
}

// ==========================================
// MÓDULO: Listar y Procesar Sistema
// ==========================================
// Ahora recibe un "booleano" para saber si debe dibujar el árbol o la lista plana
static void listar_procesos(GtkTreeStore *modelo, const char *filtro, gboolean modo_arbol) {
    struct dirent *entrada;
    DIR *directorio = opendir("/proc");

    if (directorio == NULL) return;

    gtk_tree_store_clear(modelo);

    long tamano_pagina = sysconf(_SC_PAGESIZE);
    long ticks_reloj = sysconf(_SC_CLK_TCK);

    Proceso *procesos = calloc(8192, sizeof(Proceso));
    if (!procesos) return;
    int total_procesos = 0;

    while ((entrada = readdir(directorio)) != NULL && total_procesos < 8192) {
        if (isdigit(entrada->d_name[0])) {
            Proceso p;
            p.pid = atoi(entrada->d_name);
            p.agregado_al_arbol = 0;
            strcpy(p.nombre, "Desconocido");
            strcpy(p.cpu, "0 s");
            strcpy(p.mem, "0.00 MB");

            char ruta[512];
            FILE *archivo;

            snprintf(ruta, sizeof(ruta), "/proc/%d/comm", p.pid);
            archivo = fopen(ruta, "r");
            if (archivo != NULL) {
                if (fgets(p.nombre, sizeof(p.nombre), archivo) != NULL) {
                    p.nombre[strcspn(p.nombre, "\n")] = 0; 
                }
                fclose(archivo);
            }

            if (filtro != NULL && strlen(filtro) > 0) {
                char pid_str[16];
                snprintf(pid_str, sizeof(pid_str), "%d", p.pid);
                if (strstr(p.nombre, filtro) == NULL && strstr(pid_str, filtro) == NULL) {
                    continue; 
                }
            }

            snprintf(ruta, sizeof(ruta), "/proc/%d/statm", p.pid);
            archivo = fopen(ruta, "r");
            if (archivo != NULL) {
                long paginas_total, paginas_rss;
                if (fscanf(archivo, "%ld %ld", &paginas_total, &paginas_rss) == 2) {
                    float memoria_mb = (float)(paginas_rss * tamano_pagina) / (1024.0 * 1024.0);
                    snprintf(p.mem, sizeof(p.mem), "%.2f MB", memoria_mb);
                }
                fclose(archivo);
            }

            p.ppid = 0;
            snprintf(ruta, sizeof(ruta), "/proc/%d/stat", p.pid);
            archivo = fopen(ruta, "r");
            if (archivo != NULL) {
                char linea[1024];
                if (fgets(linea, sizeof(linea), archivo) != NULL) {
                    char *cierre = strrchr(linea, ')'); 
                    if (cierre != NULL) {
                        unsigned long utime = 0, stime = 0;
                        sscanf(cierre + 2, "%*c %d %*d %*d %*d %*d %*u %*u %*u %*u %*u %lu %lu", &p.ppid, &utime, &stime);
                        snprintf(p.cpu, sizeof(p.cpu), "%lu s", (utime + stime) / ticks_reloj);
                    }
                }
                fclose(archivo);
            }

            procesos[total_procesos++] = p;
        }
    }
    closedir(directorio);

    // DIBUJO INTELIGENTE: Si el botón de árbol está activo Y no hay texto en el buscador, hacemos árbol
    gboolean usar_arbol = modo_arbol && (filtro == NULL || strlen(filtro) == 0);

    if (!usar_arbol) {
        // MODO LISTA PLANA
        for (int i = 0; i < total_procesos; i++) {
            GtkTreeIter iter;
            gtk_tree_store_append(modelo, &iter, NULL); 
            char pid_str[16];
            snprintf(pid_str, sizeof(pid_str), "%d", procesos[i].pid);
            gtk_tree_store_set(modelo, &iter,
                               COLUMNA_PID, pid_str, COLUMNA_NOMBRE, procesos[i].nombre,
                               COLUMNA_CPU, procesos[i].cpu, COLUMNA_MEMORIA, procesos[i].mem, -1);
        }
    } else {
        // MODO ÁRBOL
        construir_arbol_recursivo(modelo, NULL, 0, procesos, total_procesos);
        for (int i = 0; i < total_procesos; i++) {
            if (!procesos[i].agregado_al_arbol) {
                construir_arbol_recursivo(modelo, NULL, procesos[i].ppid, procesos, total_procesos);
            }
        }
    }

    free(procesos); 
}

// ==========================================
// MÓDULO: Actualizar Vista Unificada
// ==========================================
// Esta función lee el estado del buscador y del botón al mismo tiempo
static void actualizar_vista(GtkWidget *widget, gpointer user_data) {
    GtkTreeStore *modelo = GTK_TREE_STORE(user_data);
    
    // Recuperamos los widgets que guardamos en la memoria del modelo
    GtkWidget *buscador = g_object_get_data(G_OBJECT(modelo), "buscador");
    GtkWidget *btn_arbol = g_object_get_data(G_OBJECT(modelo), "btn_arbol");

    const char *texto = gtk_entry_get_text(GTK_ENTRY(buscador));
    gboolean modo_arbol = gtk_toggle_button_get_active(GTK_TOGGLE_BUTTON(btn_arbol));

    listar_procesos(modelo, texto, modo_arbol);
}

// ==========================================
// MÓDULO: Acciones (Finalizar, Suspender, Reanudar)
// ==========================================
static void ejecutar_accion_proceso(GtkWidget *item, gpointer user_data) {
    GtkTreeView *arbol = GTK_TREE_VIEW(user_data);
    GtkTreeSelection *seleccion = gtk_tree_view_get_selection(arbol);
    GtkTreeModel *modelo;
    GtkTreeIter iterador;

    if (gtk_tree_selection_get_selected(seleccion, &modelo, &iterador)) {
        gchar *pid_str;
        gtk_tree_model_get(modelo, &iterador, COLUMNA_PID, &pid_str, -1);
        int pid = atoi(pid_str);
        g_free(pid_str);

        const char *accion = g_object_get_data(G_OBJECT(item), "accion");

        if (strcmp(accion, "matar") == 0) {
            kill(pid, SIGKILL);
        } else if (strcmp(accion, "suspender") == 0) {
            kill(pid, SIGSTOP);
        } else if (strcmp(accion, "reanudar") == 0) {
            kill(pid, SIGCONT);
        }

        // Refrescar usando la función unificada
        actualizar_vista(NULL, modelo);
    }
}

// ==========================================
// MÓDULO: Menú de clic derecho
// ==========================================
static gboolean mostrar_menu_contextual(GtkWidget *arbol, GdkEventButton *evento, gpointer user_data) {
    if (evento->type == GDK_BUTTON_PRESS && evento->button == 3) {
        GtkTreePath *ruta;
        if (gtk_tree_view_get_path_at_pos(GTK_TREE_VIEW(arbol), evento->x, evento->y, &ruta, NULL, NULL, NULL)) {
            gtk_tree_view_set_cursor(GTK_TREE_VIEW(arbol), ruta, NULL, FALSE);
            gtk_tree_path_free(ruta);
        }

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

    GtkTreeStore *modelo = gtk_tree_store_new(NUM_COLUMNAS, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING, G_TYPE_STRING);

    // Caja horizontal para poner el buscador y el botón juntos
    GtkWidget *caja_controles = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    
    GtkWidget *buscador = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(buscador), "Buscar por nombre o PID...");
    
    GtkWidget *btn_arbol = gtk_toggle_button_new_with_label("Ver como Árbol");
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(btn_arbol), FALSE); // Inicia en modo plano

    // Guardamos los controles "dentro" del modelo para que la función actualizar_vista pueda leerlos
    g_object_set_data(G_OBJECT(modelo), "buscador", buscador);
    g_object_set_data(G_OBJECT(modelo), "btn_arbol", btn_arbol);

    // Conectamos ambos a la misma función unificada
    g_signal_connect(buscador, "changed", G_CALLBACK(actualizar_vista), modelo);
    g_signal_connect(btn_arbol, "toggled", G_CALLBACK(actualizar_vista), modelo);

    // Empaquetamos
    gtk_box_pack_start(GTK_BOX(caja_controles), buscador, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(caja_controles), btn_arbol, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja), caja_controles, FALSE, FALSE, 0);

    GtkWidget *arbol = gtk_tree_view_new_with_model(GTK_TREE_MODEL(modelo));
    g_object_unref(modelo); 

    g_signal_connect(arbol, "button-press-event", G_CALLBACK(mostrar_menu_contextual), NULL);

    configurar_columnas(GTK_TREE_VIEW(arbol));
    listar_procesos(modelo, NULL, FALSE); // Carga inicial plana

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_container_add(GTK_CONTAINER(scroll), arbol);

    gtk_box_pack_start(GTK_BOX(caja), scroll, TRUE, TRUE, 0);

    return caja;
}