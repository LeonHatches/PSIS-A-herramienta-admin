#include "archivos.h"
#include "gestor_archivos.h"
#include "gestor_respaldos.h"

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>

/* ---------------------------------------------------------------------
 * Contexto privado del módulo.
 *
 * Se agrupa todo el estado de la pantalla (widgets, modelo de datos,
 * ruta actual) en una sola estructura en vez de usar variables globales.
 * Esto respeta la separación de responsabilidades: archivos.c solo conoce
 * la interfaz, mientras que gestor_archivos.c y gestor_respaldos.c
 * concentran la lógica de disco y de respaldos.
 * --------------------------------------------------------------------- */
typedef struct {
    GtkWidget    *entrada_ruta;
    GtkWidget    *entrada_busqueda;
    GtkWidget    *vista;
    GtkListStore *modelo;
    GtkTreeModel *modelo_filtrado;
    GtkWidget    *label_stats;
    char          ruta_actual[PATH_MAX];
} ContextoArchivos;

static void contexto_archivos_destruir(gpointer datos) {
    g_free(datos);
}

/* ---------- Helpers internos ---------- */

static void aplicar_clase(GtkWidget *widget, const char *clase) {
    gtk_style_context_add_class(gtk_widget_get_style_context(widget), clase);
}

static void mostrar_mensaje(GtkWidget *widget_origen, GtkMessageType tipo, const char *texto) {
    GtkWidget *dialogo = gtk_message_dialog_new(
        GTK_WINDOW(gtk_widget_get_toplevel(widget_origen)),
        GTK_DIALOG_MODAL,
        tipo, GTK_BUTTONS_OK, "%s", texto
    );
    gtk_dialog_run(GTK_DIALOG(dialogo));
    gtk_widget_destroy(dialogo);
}

/* Vuelve a leer 'ctx->ruta_actual' del disco y refresca tabla + estadísticas */
static void refrescar_tabla(ContextoArchivos *ctx) {
    int total = cargar_archivos_reales(ctx->modelo, ctx->ruta_actual);

    char texto[PATH_MAX + 64];
    if (total < 0) {
        snprintf(texto, sizeof(texto), "Ruta inválida o sin permisos: %s", ctx->ruta_actual);
    } else {
        snprintf(texto, sizeof(texto), "Archivos listados: %d", total);
    }

    gtk_label_set_text(GTK_LABEL(ctx->label_stats), texto);
    gtk_entry_set_text(GTK_ENTRY(ctx->entrada_ruta), ctx->ruta_actual);
}

/* Cambia la carpeta activa y refresca la tabla */
static void navegar_a(ContextoArchivos *ctx, const char *ruta_nueva) {
    strncpy(ctx->ruta_actual, ruta_nueva, sizeof(ctx->ruta_actual) - 1);
    ctx->ruta_actual[sizeof(ctx->ruta_actual) - 1] = '\0';
    refrescar_tabla(ctx);
}

/* Obtiene ruta completa y tipo de la fila seleccionada en la tabla.
 * Devuelve TRUE si había una fila seleccionada. */
static gboolean obtener_seleccion(ContextoArchivos *ctx, char *ruta_out, size_t tam_ruta,
                                   char *tipo_out, size_t tam_tipo) {
    GtkTreeSelection *seleccion = gtk_tree_view_get_selection(GTK_TREE_VIEW(ctx->vista));
    GtkTreeModel *modelo;
    GtkTreeIter iter;

    if (!gtk_tree_selection_get_selected(seleccion, &modelo, &iter)) {
        return FALSE;
    }

    gchar *ruta = NULL;
    gchar *tipo = NULL;
    gtk_tree_model_get(modelo, &iter, COL_RUTA, &ruta, COL_TIPO, &tipo, -1);

    snprintf(ruta_out, tam_ruta, "%s", ruta ? ruta : "");
    if (tipo_out) snprintf(tipo_out, tam_tipo, "%s", tipo ? tipo : "");

    g_free(ruta);
    g_free(tipo);
    return TRUE;
}

/* ---------- Filtro de búsqueda (GtkTreeModelFilter) ---------- */

static gboolean fila_visible_segun_busqueda(GtkTreeModel *modelo, GtkTreeIter *iter, gpointer datos) {
    ContextoArchivos *ctx = (ContextoArchivos *) datos;
    const char *texto_busqueda = gtk_entry_get_text(GTK_ENTRY(ctx->entrada_busqueda));

    if (texto_busqueda == NULL || texto_busqueda[0] == '\0') {
        return TRUE;
    }

    gchar *nombre = NULL;
    gtk_tree_model_get(modelo, iter, COL_NOMBRE, &nombre, -1);
    if (!nombre) return FALSE;

    gchar *nombre_minusc = g_utf8_strdown(nombre, -1);
    gchar *busqueda_minusc = g_utf8_strdown(texto_busqueda, -1);

    gboolean coincide = (strstr(nombre_minusc, busqueda_minusc) != NULL);

    g_free(nombre);
    g_free(nombre_minusc);
    g_free(busqueda_minusc);
    return coincide;
}

static void on_busqueda_cambiada(GtkSearchEntry *entrada, gpointer datos) {
    (void) entrada;
    ContextoArchivos *ctx = (ContextoArchivos *) datos;
    gtk_tree_model_filter_refilter(GTK_TREE_MODEL_FILTER(ctx->modelo_filtrado));
}

/* ---------- Navegación ---------- */

static void on_ruta_activada(GtkEntry *entrada, gpointer datos) {
    ContextoArchivos *ctx = (ContextoArchivos *) datos;
    navegar_a(ctx, gtk_entry_get_text(entrada));
}

/* Doble clic (o Enter) sobre una fila: si es carpeta, se navega dentro de ella */
static void on_fila_activada(GtkTreeView *vista, GtkTreePath *path,
                              GtkTreeViewColumn *columna, gpointer datos) {
    (void) vista;
    (void) columna;
    ContextoArchivos *ctx = (ContextoArchivos *) datos;
    GtkTreeIter iter;

    if (!gtk_tree_model_get_iter(ctx->modelo_filtrado, &iter, path)) {
        return;
    }

    gchar *ruta = NULL;
    gchar *tipo = NULL;
    gtk_tree_model_get(ctx->modelo_filtrado, &iter, COL_RUTA, &ruta, COL_TIPO, &tipo, -1);

    if (tipo && strcmp(tipo, "Carpeta") == 0 && ruta) {
        navegar_a(ctx, ruta);
    }

    g_free(ruta);
    g_free(tipo);
}

/* ---------- Botones de acción ---------- */

static void on_btn_copiar_clicked(GtkWidget *widget, gpointer datos) {
    ContextoArchivos *ctx = (ContextoArchivos *) datos;
    char ruta_origen[PATH_MAX];

    if (!obtener_seleccion(ctx, ruta_origen, sizeof(ruta_origen), NULL, 0)) {
        mostrar_mensaje(widget, GTK_MESSAGE_WARNING, "Seleccione primero un archivo o carpeta.");
        return;
    }

    GtkWidget *selector = gtk_file_chooser_dialog_new(
        "Seleccione la carpeta destino",
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "Cancelar", GTK_RESPONSE_CANCEL,
        "Copiar aquí", GTK_RESPONSE_ACCEPT,
        NULL
    );

    if (gtk_dialog_run(GTK_DIALOG(selector)) == GTK_RESPONSE_ACCEPT) {
        char *carpeta_destino = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(selector));
        const char *nombre_base = strrchr(ruta_origen, '/');
        nombre_base = nombre_base ? nombre_base + 1 : ruta_origen;

        char ruta_destino[PATH_MAX * 2];
        snprintf(ruta_destino, sizeof(ruta_destino), "%s/%s", carpeta_destino, nombre_base);

        if (copiar_elemento(ruta_origen, ruta_destino) == 0) {
            refrescar_tabla(ctx);
        } else {
            mostrar_mensaje(widget, GTK_MESSAGE_ERROR, "No se pudo completar la copia.");
        }
        g_free(carpeta_destino);
    }
    gtk_widget_destroy(selector);
}

static void on_btn_mover_clicked(GtkWidget *widget, gpointer datos) {
    ContextoArchivos *ctx = (ContextoArchivos *) datos;
    char ruta_origen[PATH_MAX];

    if (!obtener_seleccion(ctx, ruta_origen, sizeof(ruta_origen), NULL, 0)) {
        mostrar_mensaje(widget, GTK_MESSAGE_WARNING, "Seleccione primero un archivo o carpeta.");
        return;
    }

    GtkWidget *selector = gtk_file_chooser_dialog_new(
        "Seleccione la carpeta destino",
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_FILE_CHOOSER_ACTION_SELECT_FOLDER,
        "Cancelar", GTK_RESPONSE_CANCEL,
        "Mover aquí", GTK_RESPONSE_ACCEPT,
        NULL
    );

    if (gtk_dialog_run(GTK_DIALOG(selector)) == GTK_RESPONSE_ACCEPT) {
        char *carpeta_destino = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(selector));
        const char *nombre_base = strrchr(ruta_origen, '/');
        nombre_base = nombre_base ? nombre_base + 1 : ruta_origen;

        char ruta_destino[PATH_MAX * 2];
        snprintf(ruta_destino, sizeof(ruta_destino), "%s/%s", carpeta_destino, nombre_base);

        if (mover_elemento(ruta_origen, ruta_destino) == 0) {
            refrescar_tabla(ctx);
        } else {
            mostrar_mensaje(widget, GTK_MESSAGE_ERROR, "No se pudo completar el movimiento.");
        }
        g_free(carpeta_destino);
    }
    gtk_widget_destroy(selector);
}

static void on_btn_eliminar_clicked(GtkWidget *widget, gpointer datos) {
    ContextoArchivos *ctx = (ContextoArchivos *) datos;
    char ruta[PATH_MAX];

    if (!obtener_seleccion(ctx, ruta, sizeof(ruta), NULL, 0)) {
        mostrar_mensaje(widget, GTK_MESSAGE_WARNING, "Seleccione primero un archivo o carpeta.");
        return;
    }

    GtkWidget *confirmacion = gtk_message_dialog_new(
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_DIALOG_MODAL,
        GTK_MESSAGE_QUESTION, GTK_BUTTONS_YES_NO,
        "¿Está seguro de eliminar:\n%s\n\nEsta acción no se puede deshacer.", ruta
    );

    gboolean confirmar = (gtk_dialog_run(GTK_DIALOG(confirmacion)) == GTK_RESPONSE_YES);
    gtk_widget_destroy(confirmacion);

    if (!confirmar) return;

    if (eliminar_elemento(ruta) == 0) {
        refrescar_tabla(ctx);
    } else {
        mostrar_mensaje(widget, GTK_MESSAGE_ERROR, "No se pudo eliminar el elemento seleccionado.");
    }
}

static void on_btn_respaldos_clicked(GtkWidget *widget, gpointer datos) {
    ContextoArchivos *ctx = (ContextoArchivos *) datos;

    GtkWidget *dialogo = gtk_dialog_new_with_buttons(
        "Gestión de Respaldos",
        GTK_WINDOW(gtk_widget_get_toplevel(widget)),
        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
        "Crear respaldo", GTK_RESPONSE_APPLY,
        "Restaurar último", GTK_RESPONSE_ACCEPT,
        "Cerrar", GTK_RESPONSE_CLOSE,
        NULL
    );

    GtkWidget *area = gtk_dialog_get_content_area(GTK_DIALOG(dialogo));

    char mensaje[PATH_MAX + 128];
    snprintf(mensaje, sizeof(mensaje),
             "\nRespaldos incrementales de:\n%s\n\nSe guardan en la carpeta '%s/' del programa.\n",
             ctx->ruta_actual, DIR_RESPALDOS);

    GtkWidget *etiqueta = gtk_label_new(mensaje);
    aplicar_clase(etiqueta, "texto");
    gtk_box_pack_start(GTK_BOX(area), etiqueta, TRUE, TRUE, 10);
    gtk_widget_show_all(dialogo);

    gint respuesta = gtk_dialog_run(GTK_DIALOG(dialogo));

    if (respuesta == GTK_RESPONSE_APPLY) {
        if (crear_respaldo_incremental(ctx->ruta_actual) == 0) {
            mostrar_mensaje(widget, GTK_MESSAGE_INFO, "Respaldo creado correctamente.");
        } else {
            mostrar_mensaje(widget, GTK_MESSAGE_ERROR,
                             "No se pudo crear el respaldo.\n¿Está instalado 'tar' en el sistema?");
        }
    } else if (respuesta == GTK_RESPONSE_ACCEPT) {
        char lista[64][256];
        int total = listar_respaldos(lista, 64);

        if (total == 0) {
            mostrar_mensaje(widget, GTK_MESSAGE_WARNING, "No hay respaldos disponibles todavía.");
        } else if (restaurar_version(lista[total - 1]) == 0) {
            mostrar_mensaje(widget, GTK_MESSAGE_INFO,
                             "Restauración completada en 'respaldos/restaurado'.");
        } else {
            mostrar_mensaje(widget, GTK_MESSAGE_ERROR, "No se pudo restaurar el respaldo.");
        }
    }

    gtk_widget_destroy(dialogo);
}

/* ---------------------------------------------------------------------
 * Construcción de la interfaz
 * --------------------------------------------------------------------- */
GtkWidget* crear_pantalla_archivos() {
    ContextoArchivos *ctx = g_new0(ContextoArchivos, 1);
    const char *inicio = g_get_home_dir();
    strncpy(ctx->ruta_actual, inicio ? inicio : ".", sizeof(ctx->ruta_actual) - 1);

    GtkWidget *caja_principal = gtk_box_new(GTK_ORIENTATION_VERTICAL, 10);
    gtk_container_set_border_width(GTK_CONTAINER(caja_principal), 10);

    /* El contexto vive mientras viva el widget principal: se libera solo,
     * sin necesidad de variables globales (separación de responsabilidades). */
    g_object_set_data_full(G_OBJECT(caja_principal), "contexto-archivos", ctx, contexto_archivos_destruir);

    /* 1. BARRA SUPERIOR: navegación por ruta + búsqueda */
    GtkWidget *caja_navegacion = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    GtkWidget *label_ruta = gtk_label_new("Ruta:");
    aplicar_clase(label_ruta, "texto");

    ctx->entrada_ruta = gtk_entry_new();
    gtk_entry_set_text(GTK_ENTRY(ctx->entrada_ruta), ctx->ruta_actual);
    g_signal_connect(ctx->entrada_ruta, "activate", G_CALLBACK(on_ruta_activada), ctx);

    ctx->entrada_busqueda = gtk_search_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(ctx->entrada_busqueda), "Buscar archivo...");
    g_signal_connect(ctx->entrada_busqueda, "search-changed", G_CALLBACK(on_busqueda_cambiada), ctx);

    gtk_box_pack_start(GTK_BOX(caja_navegacion), label_ruta, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja_navegacion), ctx->entrada_ruta, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(caja_navegacion), ctx->entrada_busqueda, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja_principal), caja_navegacion, FALSE, FALSE, 0);

    /* 2. BARRA DE BOTONES DE ACCIÓN (clases exactas del CSS corporativo) */
    GtkWidget *caja_botones = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);

    GtkWidget *btn_copiar    = gtk_button_new_with_label("Copiar");
    GtkWidget *btn_mover     = gtk_button_new_with_label("Mover");
    GtkWidget *btn_eliminar  = gtk_button_new_with_label("Eliminar");
    GtkWidget *btn_respaldos = gtk_button_new_with_label("Respaldos");

    /* Copiar/Mover/Respaldos son acciones normales -> clase "accion".
     * Eliminar es una acción destructiva/irreversible -> clase "peligro". */
    aplicar_clase(btn_copiar, "accion");
    aplicar_clase(btn_mover, "accion");
    aplicar_clase(btn_eliminar, "peligro");
    aplicar_clase(btn_respaldos, "accion");

    g_signal_connect(btn_copiar, "clicked", G_CALLBACK(on_btn_copiar_clicked), ctx);
    g_signal_connect(btn_mover, "clicked", G_CALLBACK(on_btn_mover_clicked), ctx);
    g_signal_connect(btn_eliminar, "clicked", G_CALLBACK(on_btn_eliminar_clicked), ctx);
    g_signal_connect(btn_respaldos, "clicked", G_CALLBACK(on_btn_respaldos_clicked), ctx);

    gtk_box_pack_start(GTK_BOX(caja_botones), btn_copiar, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja_botones), btn_mover, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja_botones), btn_eliminar, FALSE, FALSE, 0);

    GtkWidget *separador = gtk_separator_new(GTK_ORIENTATION_VERTICAL);
    gtk_box_pack_start(GTK_BOX(caja_botones), separador, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(caja_botones), btn_respaldos, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(caja_principal), caja_botones, FALSE, FALSE, 0);

    /* 3. TABLA DE ARCHIVOS, envuelta en una "tarjeta" (clase del CSS) */
    GtkWidget *tarjeta_tabla = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    aplicar_clase(tarjeta_tabla, "tarjeta");

    ctx->modelo = gtk_list_store_new(NUM_COLS_ARCHIVOS,
                                      G_TYPE_STRING,  /* COL_NOMBRE */
                                      G_TYPE_STRING,  /* COL_TAMANO */
                                      G_TYPE_STRING,  /* COL_TIPO   */
                                      G_TYPE_STRING); /* COL_RUTA (oculta) */

    ctx->modelo_filtrado = gtk_tree_model_filter_new(GTK_TREE_MODEL(ctx->modelo), NULL);
    gtk_tree_model_filter_set_visible_func(GTK_TREE_MODEL_FILTER(ctx->modelo_filtrado),
                                            fila_visible_segun_busqueda, ctx, NULL);

    ctx->vista = gtk_tree_view_new_with_model(ctx->modelo_filtrado);
    g_object_unref(ctx->modelo);
    g_object_unref(ctx->modelo_filtrado);

    GtkCellRenderer *renderer_nombre = gtk_cell_renderer_text_new();
    GtkCellRenderer *renderer_tamano = gtk_cell_renderer_text_new();
    GtkCellRenderer *renderer_tipo   = gtk_cell_renderer_text_new();

    GtkTreeViewColumn *col_nombre = gtk_tree_view_column_new_with_attributes(
        "Nombre", renderer_nombre, "text", COL_NOMBRE, NULL);
    GtkTreeViewColumn *col_tamano = gtk_tree_view_column_new_with_attributes(
        "Tamaño", renderer_tamano, "text", COL_TAMANO, NULL);
    GtkTreeViewColumn *col_tipo = gtk_tree_view_column_new_with_attributes(
        "Tipo", renderer_tipo, "text", COL_TIPO, NULL);

    gtk_tree_view_column_set_expand(col_nombre, TRUE);
    gtk_tree_view_column_set_resizable(col_nombre, TRUE);

    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->vista), col_nombre);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->vista), col_tamano);
    gtk_tree_view_append_column(GTK_TREE_VIEW(ctx->vista), col_tipo);

    /* Doble clic sobre una fila -> navega si es una carpeta */
    g_signal_connect(ctx->vista, "row-activated", G_CALLBACK(on_fila_activada), ctx);

    GtkWidget *scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scroll), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_container_add(GTK_CONTAINER(scroll), ctx->vista);

    gtk_box_pack_start(GTK_BOX(tarjeta_tabla), scroll, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(caja_principal), tarjeta_tabla, TRUE, TRUE, 0);

    /* 4. BARRA DE ESTADÍSTICAS (siempre en la parte inferior del layout) */
    GtkWidget *caja_estadisticas = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 5);
    ctx->label_stats = gtk_label_new("Archivos listados: 0");
    aplicar_clase(ctx->label_stats, "texto");
    gtk_box_pack_start(GTK_BOX(caja_estadisticas), ctx->label_stats, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(caja_principal), caja_estadisticas, FALSE, FALSE, 0);

    /* Carga inicial de la carpeta de inicio */
    refrescar_tabla(ctx);

    return caja_principal;
}