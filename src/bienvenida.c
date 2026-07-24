#include "bienvenida.h"

typedef struct {
    GtkStack *stack;
    gchar *nombre_modulo;
} Navegacion;

static void navegar(GtkWidget *widget, gpointer datos) {
    Navegacion *navegacion = datos;
    (void)widget;
    gtk_stack_set_visible_child_name(
        navegacion->stack, navegacion->nombre_modulo);
}

static void liberar_navegacion(gpointer datos, GClosure *cierre) {
    Navegacion *navegacion = datos;
    (void)cierre;
    g_free(navegacion->nombre_modulo);
    g_free(navegacion);
}

static GtkWidget *crear_tarjeta_modulo(
        GtkStack *stack, const gchar *icono, const gchar *titulo,
        const gchar *descripcion, const gchar *nombre_modulo) {
    GtkWidget *tarjeta = gtk_button_new();
    GtkWidget *contenido = gtk_box_new(GTK_ORIENTATION_VERTICAL, 8);
    GtkWidget *imagen = gtk_image_new_from_icon_name(icono, GTK_ICON_SIZE_DIALOG);
    GtkWidget *nombre = gtk_label_new(titulo);
    GtkWidget *detalle = gtk_label_new(descripcion);
    GtkWidget *accion = gtk_label_new("Abrir módulo  →");
    GtkWidget *espacio = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);

    gtk_style_context_add_class(
        gtk_widget_get_style_context(tarjeta), "tarjeta-modulo");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(imagen), "modulo-icono");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(nombre), "modulo-nombre");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(detalle), "modulo-detalle");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(accion), "modulo-accion");

    gtk_widget_set_size_request(tarjeta, 200, 150);
    gtk_widget_set_hexpand(tarjeta, TRUE);
    gtk_widget_set_vexpand(tarjeta, TRUE);
    gtk_image_set_pixel_size(GTK_IMAGE(imagen), 64);
    gtk_widget_set_halign(imagen, GTK_ALIGN_START);
    gtk_label_set_xalign(GTK_LABEL(nombre), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(detalle), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(accion), 0.0f);
    gtk_label_set_line_wrap(GTK_LABEL(detalle), TRUE);
    gtk_label_set_max_width_chars(GTK_LABEL(detalle), 25);

    gtk_box_pack_start(GTK_BOX(contenido), imagen, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(contenido), nombre, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(contenido), detalle, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(contenido), espacio, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(contenido), accion, FALSE, FALSE, 0);
    gtk_container_add(GTK_CONTAINER(tarjeta), contenido);

    Navegacion *navegacion = g_new0(Navegacion, 1);
    navegacion->stack = stack;
    navegacion->nombre_modulo = g_strdup(nombre_modulo);
    g_signal_connect_data(tarjeta, "clicked", G_CALLBACK(navegar), navegacion,
                          liberar_navegacion, 0);
    return tarjeta;
}

static GtkWidget *crear_indicador(
        const gchar *icono, const gchar *titulo, const gchar *valor) {
    GtkWidget *indicador = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 9);
    GtkWidget *imagen = gtk_image_new_from_icon_name(icono, GTK_ICON_SIZE_MENU);
    GtkWidget *textos = gtk_box_new(GTK_ORIENTATION_VERTICAL, 1);
    GtkWidget *etiqueta = gtk_label_new(titulo);
    GtkWidget *contenido = gtk_label_new(valor);

    gtk_style_context_add_class(
        gtk_widget_get_style_context(indicador), "indicador-inicio");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(imagen), "indicador-icono");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(etiqueta), "indicador-etiqueta");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(contenido), "indicador-valor");
    gtk_label_set_xalign(GTK_LABEL(etiqueta), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(contenido), 0.0f);
    gtk_image_set_pixel_size(GTK_IMAGE(imagen), 25);
    gtk_box_pack_start(GTK_BOX(textos), etiqueta, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(textos), contenido, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(indicador), imagen, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(indicador), textos, TRUE, TRUE, 0);
    return indicador;
}

GtkWidget *crear_pantalla_bienvenida(GtkStack *stack) {
    GtkWidget *pagina = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    GtkWidget *panel = gtk_box_new(GTK_ORIENTATION_VERTICAL, 18);
    GtkWidget *encabezado = gtk_box_new(GTK_ORIENTATION_VERTICAL, 4);
    GtkWidget *titulo = gtk_label_new("Centro de administración");
    GtkWidget *descripcion = gtk_label_new(
        "Supervisa y administra tu sistema Linux desde un solo lugar.");
    GtkWidget *indicadores = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 10);
    GtkWidget *seccion = gtk_label_new("Módulos disponibles");
    GtkWidget *fila_uno = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    GtkWidget *fila_dos = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 12);
    const gchar *usuario = g_get_user_name();
    const gchar *equipo = g_get_host_name();

    gtk_style_context_add_class(
        gtk_widget_get_style_context(pagina), "dashboard-inicio");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(panel), "dashboard-panel");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(titulo), "dashboard-titulo");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(descripcion), "dashboard-descripcion");
    gtk_style_context_add_class(
        gtk_widget_get_style_context(seccion), "dashboard-seccion");

    gtk_container_set_border_width(GTK_CONTAINER(pagina), 28);
    gtk_widget_set_halign(panel, GTK_ALIGN_FILL);
    gtk_widget_set_valign(panel, GTK_ALIGN_FILL);
    gtk_widget_set_hexpand(panel, TRUE);
    gtk_widget_set_vexpand(panel, TRUE);
    gtk_label_set_xalign(GTK_LABEL(titulo), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(descripcion), 0.0f);
    gtk_label_set_xalign(GTK_LABEL(seccion), 0.0f);

    gtk_box_pack_start(GTK_BOX(encabezado), titulo, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(encabezado), descripcion, FALSE, FALSE, 0);

    gtk_box_pack_start(GTK_BOX(indicadores), crear_indicador(
        "avatar-default-symbolic", "Usuario", usuario != NULL ? usuario : "Linux"),
        TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(indicadores), crear_indicador(
        "computer-symbolic", "Equipo", equipo != NULL ? equipo : "Sistema Linux"),
        TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(indicadores), crear_indicador(
        "emblem-default-symbolic", "Estado", "Sistema listo"), TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(fila_uno), crear_tarjeta_modulo(
        stack, "folder-symbolic", "Archivos", "Copia, mueve, elimina y busca archivos.",
        "archivos"), TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(fila_uno), crear_tarjeta_modulo(
        stack, "utilities-terminal-symbolic", "Comandos Linux",
        "Ejecuta comandos y revisa salida, errores e historial.", "consola"),
        TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(fila_uno), crear_tarjeta_modulo(
        stack, "text-x-generic-symbolic", "Analizador Bash",
        "Detecta variables y ciclos dentro de tus scripts.", "bash"),
        TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(fila_dos), crear_tarjeta_modulo(
        stack, "view-list-symbolic", "Administrador de tareas",
        "Consulta procesos, memoria y actividad del sistema.", "tareas"),
        TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(fila_dos), crear_tarjeta_modulo(
        stack, "folder-download-symbolic", "Descargas",
        "Gestiona enlaces, progreso y eventos de descarga.", "descargas"),
        TRUE, TRUE, 0);

    gtk_box_pack_start(GTK_BOX(panel), encabezado, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel), indicadores, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel), seccion, FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(panel), fila_uno, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(panel), fila_dos, TRUE, TRUE, 0);
    gtk_box_pack_start(GTK_BOX(pagina), panel, TRUE, TRUE, 0);
    return pagina;
}
