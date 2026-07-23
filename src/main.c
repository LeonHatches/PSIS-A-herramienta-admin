#include <gtk/gtk.h>
#include "bienvenida.h"
#include "archivos.h"
#include "consola.h"
#include "bash.h"
#include "tareas.h"
#include "descargas.h"

static void cargar_css() {
    GtkCssProvider *provider = gtk_css_provider_new();
    gtk_css_provider_load_from_path(provider, "assets/estilos.css", NULL);
    gtk_style_context_add_provider_for_screen(
        gdk_screen_get_default(),
        GTK_STYLE_PROVIDER(provider),
        GTK_STYLE_PROVIDER_PRIORITY_APPLICATION
    );
    g_object_unref(provider);
}

static void activar(GtkApplication *app, gpointer user_data) {
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", FALSE, NULL);
    GtkWidget *ventana = gtk_application_window_new(app);
    gtk_window_set_title(GTK_WINDOW(ventana), "Herramienta de Administrador");
    gtk_window_set_default_size(GTK_WINDOW(ventana), 1000, 600);

    cargar_css();

    GtkWidget *caja_principal = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
    gtk_container_add(GTK_CONTAINER(ventana), caja_principal);

    GtkWidget *stack = gtk_stack_new();
    gtk_stack_set_transition_type(GTK_STACK(stack), GTK_STACK_TRANSITION_TYPE_CROSSFADE);
    
    GtkWidget *sidebar = gtk_stack_sidebar_new();
    gtk_stack_sidebar_set_stack(GTK_STACK_SIDEBAR(sidebar), GTK_STACK(stack));
    gtk_widget_set_size_request(sidebar, 220, -1); 

    gtk_box_pack_start(GTK_BOX(caja_principal), sidebar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja_principal), stack, TRUE, TRUE, 0);

    gtk_stack_add_titled(GTK_STACK(stack), crear_pantalla_bienvenida(), "bienvenida", "Inicio");
    gtk_stack_add_titled(GTK_STACK(stack), crear_pantalla_archivos(), "archivos", "Archivos");
    gtk_stack_add_titled(GTK_STACK(stack), crear_pantalla_consola(), "consola", "Comandos Linux");
    gtk_stack_add_titled(GTK_STACK(stack), crear_pantalla_bash(), "bash", "Analizador Bash");
    gtk_stack_add_titled(GTK_STACK(stack), crear_pantalla_tareas(), "tareas", "Administrador de Tareas");
    gtk_stack_add_titled(GTK_STACK(stack), crear_pantalla_descargas(), "descargas", "Descargas");

    gtk_widget_show_all(ventana);
}

int main(int argc, char **argv) {
    setenv("GTK_THEME", "Adwaita", 1);
    GtkApplication *app = gtk_application_new("com.unsa.admin", G_APPLICATION_DEFAULT_FLAGS);
    g_signal_connect(app, "activate", G_CALLBACK(activar), NULL);
    int estado = g_application_run(G_APPLICATION(app), argc, argv);
    g_object_unref(app);
    
    return estado;
}