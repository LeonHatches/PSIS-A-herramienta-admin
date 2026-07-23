#include "descargas.h"
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

typedef struct {
    int id;
    char url[512];
    GtkWidget *barra;
    GtkWidget *etiqueta;
    GtkWidget *btn_cancelar;
    double progreso;
    int cancelado;
    int en_espera;
} Descarga;

static int contador_descargas = 1;
static int descargas_activas = 0;

// ==========================================
// MÓDULO: EVENTOS Y UI
// ==========================================
static gboolean actualizar_ui(gpointer user_data) {
    Descarga *d = (Descarga *)user_data;
    char texto[1024];

    if (d->cancelado) {
        gtk_label_set_text(GTK_LABEL(d->etiqueta), "Descarga Cancelada");
        gtk_widget_set_sensitive(d->btn_cancelar, FALSE);
        return G_SOURCE_REMOVE;
    }

    if (d->en_espera) {
        gtk_label_set_text(GTK_LABEL(d->etiqueta), "En espera de turno (Cola)...");
        return G_SOURCE_CONTINUE;
    }

    snprintf(texto, sizeof(texto), "Descargando: %s - %d%%", d->url, (int)(d->progreso * 100));
    gtk_label_set_text(GTK_LABEL(d->etiqueta), texto);
    gtk_progress_bar_set_fraction(GTK_PROGRESS_BAR(d->barra), d->progreso);

    if (d->progreso >= 1.0) {
        gtk_label_set_text(GTK_LABEL(d->etiqueta), "¡Descarga Completada!");
        gtk_widget_set_sensitive(d->btn_cancelar, FALSE);
        return G_SOURCE_REMOVE;
    }

    return G_SOURCE_CONTINUE;
}

// ==========================================
// MÓDULO: HILOS Y COLA (LÓGICA)
// ==========================================
static void* procesar_descarga(void *arg) {
    Descarga *d = (Descarga *)arg;

    while (descargas_activas >= 2) {
        usleep(500000); 
        if (d->cancelado) pthread_exit(NULL);
    }

    descargas_activas++;
    d->en_espera = 0;
    g_idle_add(actualizar_ui, d);

    char comando[1024];
    snprintf(comando, sizeof(comando), "wget -c -P ~/Descargas \"%s\" 2>&1", d->url);
    
    FILE *fp = popen(comando, "r");
    if (fp != NULL) {
        char linea[256];
        while (fgets(linea, sizeof(linea), fp) != NULL && !d->cancelado) {
            char *ptr = strchr(linea, '%');
            if (ptr != NULL) {
                char *inicio = ptr - 1;
                while (inicio > linea && isdigit(*inicio)) inicio--;
                int porcentaje = atoi(inicio + 1);
                d->progreso = porcentaje / 100.0;
                g_idle_add(actualizar_ui, d);
            }
        }
        pclose(fp);
    }

    if (!d->cancelado) d->progreso = 1.0;
    g_idle_add(actualizar_ui, d);
    descargas_activas--;
    
    pthread_exit(NULL);
}

// ==========================================
// MÓDULO: CONTROL DE DESCARGAS
// ==========================================
static void cancelar_descarga(GtkWidget *boton, gpointer user_data) {
    Descarga *d = (Descarga *)user_data;
    d->cancelado = 1;
}

static void agregar_descarga(GtkWidget *boton, gpointer user_data) {
    GtkWidget **widgets = (GtkWidget **)user_data;
    GtkWidget *caja_lista = widgets[0];
    GtkEntry *buscador_url = GTK_ENTRY(widgets[1]);

    const char *url = gtk_entry_get_text(buscador_url);
    if (strlen(url) < 5) return; 

    Descarga *nueva = malloc(sizeof(Descarga));
    nueva->id = contador_descargas++;
    strncpy(nueva->url, url, sizeof(nueva->url) - 1);
    nueva->progreso = 0.0;
    nueva->cancelado = 0;
    nueva->en_espera = 1; 

    GtkWidget *fila = gtk_box_new(GTK_ORIENTATION_VERTICAL, 5);
    gtk_style_context_add_class(gtk_widget_get_style_context(fila), "tarjeta");

    nueva->etiqueta = gtk_label_new("En espera de turno (Cola)...");
    gtk_widget_set_halign(nueva->etiqueta, GTK_ALIGN_START);
    
    nueva->barra = gtk_progress_bar_new();

    nueva->btn_cancelar = gtk_button_new_with_label("Cancelar");
    gtk_style_context_add_class(gtk_widget_get_style_context(nueva->btn_cancelar), "peligro");
    g_signal_connect(nueva->btn_cancelar, "clicked", G_CALLBACK(cancelar_descarga), nueva);

    gtk_box_pack_start(GTK_BOX(fila), nueva->etiqueta, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(fila), nueva->barra, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(fila), nueva->btn_cancelar, FALSE, FALSE, 0);
    
    gtk_box_pack_start(GTK_BOX(caja_lista), fila, FALSE, FALSE, 0);
    gtk_widget_show_all(fila);

    gtk_entry_set_text(buscador_url, "");

    pthread_t hilo;
    pthread_create(&hilo, NULL, procesar_descarga, nueva);
    pthread_detach(hilo);
}

// ==========================================
// MÓDULO: ENSAMBLAJE PRINCIPAL
// ==========================================
GtkWidget* crear_pantalla_descargas() {
    GtkWidget *caja_principal = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(caja_principal), 20);

    GtkWidget *titulo = gtk_label_new("Gestor de Descargas");
    gtk_style_context_add_class(gtk_widget_get_style_context(titulo), "titulo");
    gtk_widget_set_halign(titulo, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(caja_principal), titulo, FALSE, FALSE, 0);

    GtkWidget *caja_input = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *input_url = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(input_url), "Pega el link directo (ej. https://.../archivo.zip)");
    gtk_box_pack_start(GTK_BOX(caja_input), input_url, TRUE, TRUE, 0);

    GtkWidget *lista_descargas = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);

    static GtkWidget *widgets[2];
    widgets[0] = lista_descargas;
    widgets[1] = input_url;

    GtkWidget *btn_agregar = gtk_button_new_with_label("Descargar");
    gtk_style_context_add_class(gtk_widget_get_style_context(btn_agregar), "accion");
    g_signal_connect(btn_agregar, "clicked", G_CALLBACK(agregar_descarga), widgets);
    gtk_box_pack_start(GTK_BOX(caja_input), btn_agregar, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(caja_principal), caja_input, FALSE, FALSE, 0);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_set_vexpand(scroll, TRUE);
    gtk_container_add(GTK_CONTAINER(scroll), lista_descargas);
    gtk_box_pack_start(GTK_BOX(caja_principal), scroll, TRUE, TRUE, 0);

    return caja_principal;
}