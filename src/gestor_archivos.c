#include "gestor_archivos.h"

#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#define TAM_BUFFER_COPIA 8192

/* ---------- Utilidades internas (privadas al módulo) ---------- */

static int es_directorio(const char *ruta) {
    struct stat st;
    if (stat(ruta, &st) != 0) return 0;
    return S_ISDIR(st.st_mode);
}

/* ---------- Lectura del sistema de archivos ---------- */

int cargar_archivos_reales(GtkListStore *modelo, const char *ruta) {
    DIR *dir;
    struct dirent *ent;
    struct stat st;
    GtkTreeIter iter;
    int contador = 0;

    gtk_list_store_clear(modelo);

    dir = opendir(ruta);
    if (dir == NULL) {
        return -1;
    }

    while ((ent = readdir(dir)) != NULL) {
        /* Se ignoran "." y ".." para no permitir bucles de navegación infinitos */
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }

        char ruta_completa[PATH_MAX];
        snprintf(ruta_completa, sizeof(ruta_completa), "%s/%s", ruta, ent->d_name);

        if (stat(ruta_completa, &st) != 0) {
            continue; /* Se omiten enlaces rotos o elementos sin permisos de lectura */
        }

        char tamano_str[32];
        formatear_tamano((long) st.st_size, tamano_str, sizeof(tamano_str));

        const char *tipo = S_ISDIR(st.st_mode) ? "Carpeta" : "Archivo";

        gtk_list_store_append(modelo, &iter);
        gtk_list_store_set(modelo, &iter,
                            COL_NOMBRE, ent->d_name,
                            COL_TAMANO, S_ISDIR(st.st_mode) ? "--" : tamano_str,
                            COL_TIPO, tipo,
                            COL_RUTA, ruta_completa,
                            -1);
        contador++;
    }

    closedir(dir);
    return contador;
}

void formatear_tamano(long bytes, char *buffer, size_t buffer_size) {
    static const char *unidades[] = {"B", "KB", "MB", "GB", "TB"};
    double tam = (double) bytes;
    int indice = 0;

    while (tam >= 1024.0 && indice < 4) {
        tam /= 1024.0;
        indice++;
    }

    if (indice == 0) {
        snprintf(buffer, buffer_size, "%ld %s", bytes, unidades[indice]);
    } else {
        snprintf(buffer, buffer_size, "%.1f %s", tam, unidades[indice]);
    }
}

/* ---------- Copiar (lectura/escritura real de bytes) ---------- */

static int copiar_archivo_simple(const char *origen, const char *destino) {
    FILE *f_origen = fopen(origen, "rb");
    if (!f_origen) return -1;

    FILE *f_destino = fopen(destino, "wb");
    if (!f_destino) {
        fclose(f_origen);
        return -1;
    }

    char buffer[TAM_BUFFER_COPIA];
    size_t leidos;
    int error = 0;

    while ((leidos = fread(buffer, 1, sizeof(buffer), f_origen)) > 0) {
        if (fwrite(buffer, 1, leidos, f_destino) != leidos) {
            error = 1;
            break;
        }
    }

    fclose(f_origen);
    fclose(f_destino);
    return error ? -1 : 0;
}

int copiar_elemento(const char *origen, const char *destino) {
    if (!es_directorio(origen)) {
        return copiar_archivo_simple(origen, destino);
    }

    /* Es una carpeta: se crea el destino (si no existe) y se copia su
     * contenido de forma recursiva, entrada por entrada. */
    if (mkdir(destino, 0755) != 0 && errno != EEXIST) {
        return -1;
    }

    DIR *dir = opendir(origen);
    if (!dir) return -1;

    struct dirent *ent;
    int resultado = 0;

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char sub_origen[PATH_MAX];
        char sub_destino[PATH_MAX];
        snprintf(sub_origen, sizeof(sub_origen), "%s/%s", origen, ent->d_name);
        snprintf(sub_destino, sizeof(sub_destino), "%s/%s", destino, ent->d_name);

        if (copiar_elemento(sub_origen, sub_destino) != 0) {
            resultado = -1;
        }
    }

    closedir(dir);
    return resultado;
}

/* ---------- Mover ---------- */

int mover_elemento(const char *origen, const char *destino) {
    /* rename() es la forma nativa y más eficiente de mover dentro del mismo
     * sistema de archivos. Si origen y destino están en discos/particiones
     * distintas, rename() falla con EXDEV y se recurre a copiar + eliminar. */
    if (rename(origen, destino) == 0) {
        return 0;
    }

    if (errno == EXDEV) {
        if (copiar_elemento(origen, destino) == 0) {
            return eliminar_elemento(origen);
        }
    }

    return -1;
}

/* ---------- Eliminar ---------- */

int eliminar_elemento(const char *ruta) {
    if (!es_directorio(ruta)) {
        return remove(ruta);
    }

    DIR *dir = opendir(ruta);
    if (!dir) return -1;

    struct dirent *ent;
    int resultado = 0;

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char sub_ruta[PATH_MAX];
        snprintf(sub_ruta, sizeof(sub_ruta), "%s/%s", ruta, ent->d_name);

        if (eliminar_elemento(sub_ruta) != 0) {
            resultado = -1;
        }
    }

    closedir(dir);

    if (rmdir(ruta) != 0) {
        resultado = -1;
    }

    return resultado;
}