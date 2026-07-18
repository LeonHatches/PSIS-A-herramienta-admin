#include "bash.h"

typedef struct {
    GtkWidget *ventana;
    GtkTextBuffer *codigo;
    GtkTextBuffer *resultado;
    GtkWidget *archivo;
} AnalizadorBash;

static void agregar_linea(GString *resultado, gint numero, const gchar *tipo,
                          const gchar *detalle) {
    g_string_append_printf(resultado, "Línea %-4d  %-10s %s\n", numero, tipo, detalle);
}

static void analizar_codigo(GtkWidget *widget, gpointer datos) {
    AnalizadorBash *analizador = datos;
    GtkTextIter inicio;
    GtkTextIter fin;
    gtk_text_buffer_get_bounds(analizador->codigo, &inicio, &fin);
    gchar *codigo = gtk_text_buffer_get_text(analizador->codigo, &inicio, &fin, FALSE);

    (void)widget;
    if (codigo[0] == '\0') {
        gtk_text_buffer_set_text(analizador->resultado,
            "Abra un script Bash o escriba su contenido antes de analizar.", -1);
        g_free(codigo);
        return;
    }

    GRegex *asignacion = g_regex_new(
        "(?:^|[;[:space:]])([A-Za-z_][A-Za-z0-9_]*)[[:space:]]*=",
        G_REGEX_OPTIMIZE, 0, NULL);
    GRegex *uso = g_regex_new(
        "\\$\\{?([A-Za-z_][A-Za-z0-9_]*)\\}?",
        G_REGEX_OPTIMIZE, 0, NULL);
    GRegex *ciclo = g_regex_new(
        "^[[:space:]]*(for|while|until|select)[[:space:]]+",
        G_REGEX_OPTIMIZE, 0, NULL);
    gchar **lineas = g_strsplit(codigo, "\n", -1);
    GString *informe = g_string_new("RESULTADO DEL ANÁLISIS\n\n");
    gint variables = 0;
    gint ciclos = 0;

    for (gint i = 0; lineas[i] != NULL; i++) {
        gchar *sin_espacios = g_strstrip(g_strdup(lineas[i]));
        if (sin_espacios[0] == '#' || sin_espacios[0] == '\0') {
            g_free(sin_espacios);
            continue;
        }
        g_free(sin_espacios);

        GMatchInfo *coincidencias = NULL;
        g_regex_match(asignacion, lineas[i], 0, &coincidencias);
        while (g_match_info_matches(coincidencias)) {
            gchar *nombre = g_match_info_fetch(coincidencias, 1);
            agregar_linea(informe, i + 1, "VARIABLE", nombre);
            variables++;
            g_free(nombre);
            g_match_info_next(coincidencias, NULL);
        }
        g_match_info_free(coincidencias);

        coincidencias = NULL;
        g_regex_match(uso, lineas[i], 0, &coincidencias);
        while (g_match_info_matches(coincidencias)) {
            gchar *nombre = g_match_info_fetch(coincidencias, 1);
            gchar *detalle = g_strdup_printf("uso de $%s", nombre);
            agregar_linea(informe, i + 1, "USO", detalle);
            variables++;
            g_free(detalle);
            g_free(nombre);
            g_match_info_next(coincidencias, NULL);
        }
        g_match_info_free(coincidencias);

        coincidencias = NULL;
        if (g_regex_match(ciclo, lineas[i], 0, &coincidencias)) {
            gchar *tipo = g_match_info_fetch(coincidencias, 1);
            agregar_linea(informe, i + 1, "CICLO", tipo);
            ciclos++;
            g_free(tipo);
        }
        g_match_info_free(coincidencias);
    }

    if (variables == 0 && ciclos == 0)
        g_string_append(informe, "No se encontraron variables ni ciclos.\n");
    g_string_append_printf(informe,
        "\nResumen: %d referencia(s) de variables y %d ciclo(s) detectado(s).",
        variables, ciclos);
    gtk_text_buffer_set_text(analizador->resultado, informe->str, -1);

    g_string_free(informe, TRUE);
    g_strfreev(lineas);
    g_regex_unref(asignacion);
    g_regex_unref(uso);
    g_regex_unref(ciclo);
    g_free(codigo);
}

static void abrir_script(GtkWidget *widget, gpointer datos) {
    AnalizadorBash *analizador = datos;
    GtkWidget *dialogo = gtk_file_chooser_dialog_new(
        "Abrir script Bash", GTK_WINDOW(analizador->ventana),
        GTK_FILE_CHOOSER_ACTION_OPEN, "Cancelar", GTK_RESPONSE_CANCEL,
        "Abrir", GTK_RESPONSE_ACCEPT, NULL);
    GtkFileFilter *filtro = gtk_file_filter_new();
    gtk_file_filter_set_name(filtro, "Scripts Bash (*.sh)");
    gtk_file_filter_add_pattern(filtro, "*.sh");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialogo), filtro);

    (void)widget;
    if (gtk_dialog_run(GTK_DIALOG(dialogo)) == GTK_RESPONSE_ACCEPT) {
        gchar *ruta = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialogo));
        gchar *contenido = NULL;
        GError *error = NULL;
        if (g_file_get_contents(ruta, &contenido, NULL, &error)) {
            gtk_text_buffer_set_text(analizador->codigo, contenido, -1);
            gtk_label_set_text(GTK_LABEL(analizador->archivo), ruta);
            g_free(contenido);
        } else {
            gtk_text_buffer_set_text(analizador->resultado, error->message, -1);
            g_clear_error(&error);
        }
        g_free(ruta);
    }
    gtk_widget_destroy(dialogo);
}

static GtkWidget *crear_editor(GtkTextBuffer **buffer) {
    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    GtkWidget *vista = gtk_text_view_new();
    gtk_text_view_set_monospace(GTK_TEXT_VIEW(vista), TRUE);
    gtk_text_view_set_wrap_mode(GTK_TEXT_VIEW(vista), GTK_WRAP_NONE);
    gtk_container_add(GTK_CONTAINER(scroll), vista);
    *buffer = gtk_text_view_get_buffer(GTK_TEXT_VIEW(vista));
    return scroll;
}

GtkWidget *crear_pantalla_bash(void) {
    AnalizadorBash *analizador = g_new0(AnalizadorBash, 1);
    GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 12);
    GtkWidget *titulo = gtk_label_new("Analizador de scripts Bash");
    GtkWidget *descripcion = gtk_label_new(
        "Abra o pegue un script para localizar variables y ciclos por número de línea.");
    GtkWidget *barra = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 8);
    GtkWidget *abrir = gtk_button_new_with_label("Abrir .sh");
    GtkWidget *analizar = gtk_button_new_with_label("Analizar script");
    GtkWidget *paneles = gtk_paned_new(GTK_ORIENTATION_HORIZONTAL);
    GtkWidget *caja_codigo = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *caja_resultado = gtk_box_new(GTK_ORIENTATION_VERTICAL, 6);
    GtkWidget *titulo_codigo = gtk_label_new("Código Bash");
    GtkWidget *titulo_resultado = gtk_label_new("Variables y ciclos detectados");
    GtkWidget *editor = crear_editor(&analizador->codigo);
    GtkWidget *visor = crear_editor(&analizador->resultado);

    gtk_container_set_border_width(GTK_CONTAINER(pagina), 24);
    gtk_label_set_xalign(GTK_LABEL(titulo), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(titulo), "titulo");
    gtk_label_set_xalign(GTK_LABEL(descripcion), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(descripcion), "texto");
    analizador->archivo = gtk_label_new("Ningún archivo seleccionado");
    gtk_label_set_ellipsize(GTK_LABEL(analizador->archivo), PANGO_ELLIPSIZE_MIDDLE);
    gtk_label_set_xalign(GTK_LABEL(analizador->archivo), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(analizar), "accion");
    gtk_box_pack_start(GTK_BOX(barra), abrir, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(barra), analizar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(barra), analizador->archivo, TRUE, TRUE, 6);

    gtk_label_set_xalign(GTK_LABEL(titulo_codigo), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(titulo_resultado), 0.0f);
    gtk_style_context_add_class(gtk_widget_get_style_context(titulo_codigo), "subtitulo");
    gtk_style_context_add_class(gtk_widget_get_style_context(titulo_resultado), "subtitulo");
    gtk_text_view_set_editable(GTK_TEXT_VIEW(gtk_bin_get_child(GTK_BIN(editor))), TRUE);
    gtk_text_view_set_editable(GTK_TEXT_VIEW(gtk_bin_get_child(GTK_BIN(visor))), FALSE);
    gtk_box_pack_start(GTK_BOX(caja_codigo), titulo_codigo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja_codigo), editor, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(caja_resultado), titulo_resultado, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja_resultado), visor, TRUE, TRUE, 0);
    gtk_paned_pack1(GTK_PANED(paneles), caja_codigo, TRUE, FALSE);
    gtk_paned_pack2(GTK_PANED(paneles), caja_resultado, TRUE, FALSE);
    gtk_widget_set_vexpand(paneles, TRUE);

    gtk_box_pack_start(GTK_BOX(pagina), titulo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pagina), descripcion, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pagina), barra, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(pagina), paneles, TRUE, TRUE, 0);

    g_signal_connect(abrir, "clicked", G_CALLBACK(abrir_script), analizador);
    g_signal_connect(analizar, "clicked", G_CALLBACK(analizar_codigo), analizador);
    g_object_set_data_full(G_OBJECT(pagina), "bash-datos", analizador, g_free);
    return pagina;
}
