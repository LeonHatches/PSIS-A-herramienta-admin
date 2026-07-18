#include "consola.h"

typedef struct {
    GtkWidget *entrada;
    GtkWidget *boton_ejecutar;
    GtkTextBuffer *salida;
    GtkTextBuffer *errores;
    GtkWidget *historial;
} Consola;

typedef struct {
    Consola *consola;
    GSubprocess *proceso;
} Ejecucion;

static void reemplazar_texto(GtkTextBuffer *buffer, const gchar *texto) {
    gtk_text_buffer_set_text(buffer, texto != NULL ? texto : "", -1);
}

static void finalizar_ejecucion(GObject *origen, GAsyncResult *resultado, gpointer datos) {
    Ejecucion *ejecucion = datos;
    gchar *salida = NULL;
    gchar *errores = NULL;
    GError *error = NULL;

    if (!g_subprocess_communicate_utf8_finish(
            G_SUBPROCESS(origen), resultado, &salida, &errores, &error)) {
        reemplazar_texto(ejecucion->consola->errores, error->message);
        g_clear_error(&error);
    } else {
        reemplazar_texto(ejecucion->consola->salida, salida);
        reemplazar_texto(ejecucion->consola->errores, errores);
    }

    gtk_widget_set_sensitive(ejecucion->consola->boton_ejecutar, TRUE);
    g_free(salida);
    g_free(errores);
    g_object_unref(ejecucion->proceso);
    g_free(ejecucion);
}

static void agregar_al_historial(Consola *consola, const gchar *comando) {
    GtkWidget *fila = gtk_list_box_row_new();
    GtkWidget *etiqueta = gtk_label_new(comando);
    gtk_label_set_xalign(GTK_LABEL(etiqueta), 0.0f);
    gtk_label_set_ellipsize(GTK_LABEL(etiqueta), PANGO_ELLIPSIZE_END);
    gtk_container_add(GTK_CONTAINER(fila), etiqueta);
    gtk_container_add(GTK_CONTAINER(consola->historial), fila);
    gtk_widget_show_all(fila);
}

static void ejecutar_comando(GtkWidget *widget, gpointer datos) {
    Consola *consola = datos;
    const gchar *comando = gtk_entry_get_text(GTK_ENTRY(consola->entrada));
    GError *error = NULL;

    (void)widget;
    if (comando == NULL || comando[0] == '\0') {
        reemplazar_texto(consola->errores, "Escriba un comando antes de ejecutar.");
        return;
    }

    GSubprocess *proceso = g_subprocess_new(
        G_SUBPROCESS_FLAGS_STDOUT_PIPE | G_SUBPROCESS_FLAGS_STDERR_PIPE,
        &error, "/bin/sh", "-c", comando, NULL);

    if (proceso == NULL) {
        reemplazar_texto(consola->errores, error->message);
        g_clear_error(&error);
        return;
    }

    reemplazar_texto(consola->salida, "Ejecutando...");
    reemplazar_texto(consola->errores, "");
    agregar_al_historial(consola, comando);
    gtk_widget_set_sensitive(consola->boton_ejecutar, FALSE);

    Ejecucion *ejecucion = g_new0(Ejecucion, 1);
    ejecucion->consola = consola;
    ejecucion->proceso = proceso;
    g_subprocess_communicate_utf8_async(
        proceso, NULL, NULL, finalizar_ejecucion, ejecucion);
}

static void seleccionar_historial(GtkListBox *lista, GtkListBoxRow *fila, gpointer datos) {
    Consola *consola = datos;
    (void)lista;
    if (fila != NULL) {
        GtkWidget *etiqueta = gtk_bin_get_child(GTK_BIN(fila));
        gtk_entry_set_text(GTK_ENTRY(consola->entrada),
                           gtk_label_get_text(GTK_LABEL(etiqueta)));
    }
}

static void limpiar_resultados(GtkWidget *widget, gpointer datos) {
    Consola *consola = datos;
    (void)widget;
    reemplazar_texto(consola->salida, "");
    reemplazar_texto(consola->errores, "");
}

static GtkWidget *crear_panel_texto(const gchar *titulo, GtkTextBuffer **buffer) {
    GtkWidget *caja = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *etiqueta = gtk_label_new(titulo);
    GtkWidget *desplazamiento = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *vista = gtk_text_view_new();

    gtk_label_set_xalign(GTK_LABEL(etiqueta), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(etiqueta), "subtitulo");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(vista), FALSE);
    gtk_text_view_set_cursor_visible(GTK_TEXT_VIEW(vista), FALSE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(vista), GTK_WRAP_WORD_CHAR);
    gtk_container_add(GTK_CONTAINER(desplazamiento), vista);
    gtk_widget_set_vexpand(desplazamiento, TRUE);
    gtk_box_pack_start(GTK_BOX(caja), etiqueta, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja), desplazamiento, TRUE, TRUE, 0);
    *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(vista));
    return caja;
}

GtkWidget *crear_pantalla_consola(void) {
    Consola *consola = g_new0(Consola, 1);
    GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *titulo = gtk_label_new("Comandos Linux");
    GtkWidget *descripcion = gtk_label_new(
        "Ejecute comandos y revise por separado la salida, los errores y el historial.");
    GtkWidget *barra = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *limpiar = gtk_button_new_with_label("Limpiar salida");
    GtkWidget *contenido = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *resultados = gtk_paned_new(GTK_ORIENTATION_VERTICAL);
    GtkWidget *panel_historial = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *titulo_historial = gtk_label_new("Historial");
    GtkWidget *scroll_historial = gtk_scrolled_window_new(NULL, NULL);

    gtk_container_set_border_width(GTK_CONTAINER(pagina), 24);
    gtk_label_set_xalign(GTK_LABEL(titulo), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(titulo), "titulo");
    gtk_label_set_xalign(GTK_LABEL(descripcion), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(descripcion), "texto");

    consola->entrada = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(consola->entrada), "Ejemplo: ls -la");
    consola->boton_ejecutar = gtk_button_new_with_label("Ejecutar");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(consola->boton_ejecutar), "accion");
    gtk_box_pack_start(GTK_BOX(barra), consola->entrada, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(barra), consola->boton_ejecutar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(barra), limpiar, FALSE, FALSE, 0);

    GtkWidget *panel_salida = crear_panel_texto("Salida", &consola->salida);
    GtkWidget *panel_errores = crear_panel_texto("Errores", &consola->errores);
    gtk_paned_pack1(GTK_PANED(resultados), panel_salida, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(resultados), panel_errores, TRUE, FALSE);

    consola->historial = gtk_list_box_new();
    gtk_list_box_set_selection_mode(GTK_LIST_BOX(consola->historial), GTK_SELECTION_SINGLE);
    gtk_label_set_xalign(GTK_LABEL(titulo_historial), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(titulo_historial), "subtitulo");
    gtk_container_add(GTK_CONTAINER(scroll_historial), consola->historial);
    gtk_box_pack_start(GTK_BOX(panel_historial), titulo_historial, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel_historial), scroll_historial, TRUE, TRUE, 0);
    gtk_widget_set_size_request(panel_historial, 230, -1);

    gtk_paned_pack1(GTK_PANED(contenido), resultados, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(contenido), panel_historial, FALSE, FALSE);
    gtk_widget_set_vexpand(contenido, TRUE);

    gtk_box_pack_start(GTK_BOX(pagina), titulo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pagina), descripcion, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pagina), barra, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pagina), contenido, TRUE, TRUE, 0);

    g_signal_connect(consola->boton_ejecutar, "clicked", G_CALLBACK(ejecutar_comando), consola);
    g_signal_connect(consola->entrada, "activate", G_CALLBACK(ejecutar_comando), consola);
    g_signal_connect(limpiar, "clicked", G_CALLBACK(limpiar_resultados), consola);
    g_signal_connect(consola->historial, "row-activated",
                     G_CALLBACK(seleccionar_historial), consola);
    g_object_set_data_full(G_OBJECT(pagina), "consola-datos", consola, g_free);

    return pagina;
}
