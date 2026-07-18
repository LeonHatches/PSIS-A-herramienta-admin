#include "bienvenida.h"

GtkWidget* crear_pantalla_bienvenida() {
    GtkWidget *tarjeta_bienvenida = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_widget_set_halign(tarjeta_bienvenida, GTK_ALIGN_CENTER);
    gtk_widget_set_valign(tarjeta_bienvenida, GTK_ALIGN_CENTER);
    gtk_style_context_add_class(gtk_widget_get_style_context(tarjeta_bienvenida), "tarjeta");

    GtkWidget *titulo = gtk_label_new("Bienvenido al Administrador");
    gtk_style_context_add_class(gtk_widget_get_style_context(titulo), "titulo");
    
    GtkWidget *texto = gtk_label_new("Seleccione un módulo en el menú lateral para comenzar.");
    gtk_style_context_add_class(gtk_widget_get_style_context(texto), "texto");

    gtk_box_pack_start(GTK_BOX(tarjeta_bienvenida), titulo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(tarjeta_bienvenida), texto, FALSE, FALSE, 0);

    return tarjeta_bienvenida;
}