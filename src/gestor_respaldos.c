#include "gestor_respaldos.h"
#include "gestor_archivos.h" /* reutilizamos eliminar_elemento(), ya probada y sin pasar por shell */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>
#include <libgen.h>
#include <limits.h>

/* Se asegura de que exista la carpeta donde se guardan los respaldos */
static void asegurar_carpeta_respaldos(void) {
    struct stat st;
    if (stat(DIR_RESPALDOS, &st) != 0) {
        mkdir(DIR_RESPALDOS, 0755);
    }
}

/* ---------------------------------------------------------------------
 * Validación de seguridad para la carpeta destino de una restauración.
 * Una restauración jamás debe poder vaciar la raíz del sistema ni una
 * carpeta crítica, aunque llegue una ruta vacía, relativa o inesperada.
 * --------------------------------------------------------------------- */
static int ruta_destino_es_segura(const char *ruta) {
    if (ruta == NULL || ruta[0] == '\0') {
        return 0;
    }

    char ruta_real[PATH_MAX];
    if (realpath(ruta, ruta_real) == NULL) {
        return 0; /* la carpeta no existe o no se pudo resolver la ruta */
    }

    static const char *rutas_prohibidas[] = {
        "/", "/home", "/root", "/etc", "/usr", "/bin", "/sbin",
        "/lib", "/lib64", "/var", "/boot", "/dev", "/proc", "/sys", "/opt"
    };

    size_t total_prohibidas = sizeof(rutas_prohibidas) / sizeof(rutas_prohibidas[0]);
    for (size_t i = 0; i < total_prohibidas; i++) {
        if (strcmp(ruta_real, rutas_prohibidas[i]) == 0) {
            return 0;
        }
    }

    return 1;
}

/* Vacía el CONTENIDO de 'ruta' (la carpeta en sí NO se elimina), dejándola
 * lista para recibir la extracción del respaldo. Se apoya en
 * eliminar_elemento() -- la misma función nativa en C que usa el botón
 * "Eliminar" de la interfaz -- en vez de invocar "rm -rf" por una shell,
 * evitando así cualquier riesgo de inyección de comandos.
 * Devuelve 0 si TODO se pudo borrar, o -1 si algún elemento falló (por
 * ejemplo, por falta de permisos); en ese caso se sigue intentando con el
 * resto de los elementos para dejar la carpeta lo más limpia posible, pero
 * se reporta el fallo para que la extracción se aborte. */
static int vaciar_carpeta(const char *ruta) {
    DIR *dir = opendir(ruta);
    if (!dir) return -1;

    struct dirent *ent;
    int resultado = 0;

    while ((ent = readdir(dir)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) continue;

        char ruta_hijo[PATH_MAX];
        snprintf(ruta_hijo, sizeof(ruta_hijo), "%s/%s", ruta, ent->d_name);

        if (eliminar_elemento(ruta_hijo) != 0) {
            resultado = -1;
        }
    }

    closedir(dir);
    return resultado;
}

int crear_respaldo_incremental(const char *ruta_origen) {
    asegurar_carpeta_respaldos();

    struct stat st_origen;
    if (stat(ruta_origen, &st_origen) != 0) {
        return -1; /* la ruta seleccionada ya no existe */
    }

    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    char marca_tiempo[32];
    strftime(marca_tiempo, sizeof(marca_tiempo), "%Y%m%d_%H%M%S", t);

    char ruta_snapshot[300];
    snprintf(ruta_snapshot, sizeof(ruta_snapshot), "%s/snapshot.snar", DIR_RESPALDOS);

    char ruta_respaldo[300];
    snprintf(ruta_respaldo, sizeof(ruta_respaldo), "%s/respaldo_%s.tar.gz", DIR_RESPALDOS, marca_tiempo);

    char comando[1200];

    if (S_ISDIR(st_origen.st_mode)) {
        /* Origen es una carpeta: se respalda su CONTENIDO.
         * --listed-incremental usa el .snar como "memoria": si no existe, tar
         * hace un respaldo completo y lo crea; si ya existe, compara metadatos
         * y solo agrega lo que cambió desde el respaldo anterior. */
        snprintf(comando, sizeof(comando),
                 "tar --listed-incremental=%s -czf %s -C %s . 2>/dev/null",
                 ruta_snapshot, ruta_respaldo, ruta_origen);
    } else {
        /* Origen es un archivo individual: 'tar -C carpeta .' no funciona
         * sobre un archivo (tar necesita cambiar a un DIRECTORIO), así que en
         * su lugar nos posicionamos en la carpeta que lo contiene y
         * empaquetamos solo su nombre. dirname()/basename() pueden modificar
         * su argumento, por eso se usan copias separadas de la ruta. */
        char copia_para_carpeta[300];
        char copia_para_nombre[300];
        snprintf(copia_para_carpeta, sizeof(copia_para_carpeta), "%s", ruta_origen);
        snprintf(copia_para_nombre, sizeof(copia_para_nombre), "%s", ruta_origen);

        char *carpeta_contenedora = dirname(copia_para_carpeta);
        char *nombre_archivo = basename(copia_para_nombre);

        snprintf(comando, sizeof(comando),
                 "tar --listed-incremental=%s -czf %s -C %s %s 2>/dev/null",
                 ruta_snapshot, ruta_respaldo, carpeta_contenedora, nombre_archivo);
    }

    system(comando);

    /* El código de salida de tar NO siempre refleja una falla real: al
     * respaldar carpetas grandes es normal que tar emita advertencias (ej.
     * sockets, archivos sin permiso de lectura) y termine con estado != 0
     * aunque el .tar.gz se haya generado correctamente. Por eso el criterio
     * de éxito real es que el archivo resultante exista y tenga contenido. */
    struct stat st_resultado;
    if (stat(ruta_respaldo, &st_resultado) == 0 && st_resultado.st_size > 0) {
        return 0;
    }
    return -1;
}

int restaurar_version(const char *archivo_respaldo, const char *ruta_destino) {
    struct stat st_respaldo;
    if (stat(archivo_respaldo, &st_respaldo) != 0) {
        return -1; /* el archivo de respaldo indicado no existe */
    }

    /* 1. Validación: nunca se limpia una ruta vacía, la raíz del sistema, o
     *    una carpeta crítica del sistema operativo. Se aborta ANTES de
     *    borrar nada si la ruta no es segura. */
    if (!ruta_destino_es_segura(ruta_destino)) {
        return -1;
    }

    /* 2. Limpieza: se vacía el contenido de ruta_destino. Si algo no se pudo
     *    borrar (ej. falta de permisos), se aborta sin extraer nada encima,
     *    para no dejar el sistema en un estado a medias (respaldo viejo
     *    mezclado con archivos parcialmente borrados). */
    if (vaciar_carpeta(ruta_destino) != 0) {
        return -1;
    }

    /* 3. Extracción: --overwrite hace explícito que tar debe sobrescribir
     *    cualquier archivo que pudiera quedar, en vez de fallar o preguntar
     *    interactivamente (lo cual colgaría el programa, ya que se ejecuta
     *    de forma no interactiva vía system()). */
    char comando[700];
    snprintf(comando, sizeof(comando),
             "tar -xzf %s -C %s --overwrite 2>/dev/null",
             archivo_respaldo, ruta_destino);

    int estado = system(comando);
    return (estado == 0) ? 0 : -1;
}

/* Comparador para qsort(): ordena alfabéticamente, que equivale a orden
 * cronológico gracias a que el nombre incluye la marca de tiempo
 * "YYYYMMDD_HHMMSS" con ceros a la izquierda. */
static int comparar_nombres(const void *a, const void *b) {
    return strcmp((const char *) a, (const char *) b);
}

int listar_respaldos(char lista[][256], int max_elementos) {
    asegurar_carpeta_respaldos();

    DIR *dir = opendir(DIR_RESPALDOS);
    if (!dir) return 0;

    int contador = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL && contador < max_elementos) {
        if (strstr(ent->d_name, ".tar.gz") != NULL) {
            snprintf(lista[contador], 256, "%.9s/%.245s", DIR_RESPALDOS, ent->d_name);
            contador++;
        }
    }

    closedir(dir);

    /* readdir() NO garantiza ningún orden: sin este qsort, "el último de la
     * lista" podría no ser el respaldo más reciente. */
    qsort(lista, contador, 256, comparar_nombres);

    return contador;
}