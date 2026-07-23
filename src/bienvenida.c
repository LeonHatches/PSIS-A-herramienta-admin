#include "bienvenida.h"
#include <stdlib.h>
#include <stdio.h>

GtkWidget* crear_pantalla_bienvenida() {
    // Contenedor principal para mantener todo centrado
    GtkWidget *contenedor = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_widget_set_halign(contenedor, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(contenedor, GTK_ALIGN_CENTER);

    // Tarjeta blanca donde irá el contenido
    GtkWidget *tarjeta = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_style_context_add_class(gtk_widget_get_style_context(tarjeta), "tarjeta");

    // 1. Icono representativo
    GtkWidget *icono = gtk_image_new_from_icon_name("preferences-system", GTK_ICON_SIZE_DIALOG);
    gtk_widget_set_margin_bottom(icono, 10);
    gtk_box_pack_start(GTK_BOX(tarjeta), icono, FALSE, FALSE, 0);

    // 2. Título personalizado (Detecta el usuario de Linux)
    char mensaje[256];
    const char *usuario = getenv("USER");
    if (usuario == NULL) {
        usuario = "Usuario";
    }
    snprintf(mensaje, sizeof(mensaje), "¡Bienvenido, %s!", usuario);

    GtkWidget *titulo = gtk_label_new(mensaje);
    gtk_style_context_add_class(gtk_widget_get_style_context(titulo), "titulo");
    gtk_box_pack_start(GTK_BOX(tarjeta), titulo, FALSE, FALSE, 0);

    // 3. Subtítulo principal
    GtkWidget *texto = gtk_label_new("Herramienta de Administración del Sistema");
    gtk_style_context_add_class(gtk_widget_get_style_context(texto), "texto");
    gtk_box_pack_start(GTK_BOX(tarjeta), texto, FALSE, FALSE, 0);

    // 4. Línea separadora
    GtkWidget *separador = gtk_separator_new(GTK_ORIENTATION_HORIZONTAL);
    gtk_widget_set_margin_top(separador, 15);
    gtk_widget_set_margin_bottom(separador, 15);
    gtk_box_pack_start(GTK_BOX(tarjeta), separador, FALSE, FALSE, 0);

    // 5. Lista de información de los módulos
    GtkWidget *info_modulos = gtk_label_new(
        "Módulos disponibles:\n\n"
        "• Archivos: Copia, mueve y elimina archivos.\n"
        "• Comandos Linux: Ejecuta procesos nativos.\n"
        "• Analizador Bash: Revisa y valida tus scripts.\n"
        "• Administrador de Tareas: Controla procesos y memoria.\n"
        "• Descargas: Gestor de descargas mediante enlaces."
    );
    gtk_widget_set_halign(info_modulos, GTK_ALIGN_START);
    gtk_box_pack_start(GTK_BOX(tarjeta), info_modulos, FALSE, FALSE, 0);

    // Ensamblaje final
    gtk_box_pack_start(GTK_BOX(contenedor), tarjeta, FALSE, FALSE, 0);

    return contenedor;
}