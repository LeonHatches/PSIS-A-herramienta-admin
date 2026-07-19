#include "gestor_respaldos.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>
#include <dirent.h>

/* Se asegura de que exista la carpeta donde se guardan los respaldos */
static void asegurar_carpeta_respaldos(void) {
    struct stat st;
    if (stat(DIR_RESPALDOS, &st) != 0) {
        mkdir(DIR_RESPALDOS, 0755);
    }
}

int crear_respaldo_incremental(const char *directorio_origen) {
    asegurar_carpeta_respaldos();

    time_t ahora = time(NULL);
    struct tm *t = localtime(&ahora);
    char marca_tiempo[32];
    strftime(marca_tiempo, sizeof(marca_tiempo), "%Y%m%d_%H%M%S", t);

    char ruta_snapshot[300];
    snprintf(ruta_snapshot, sizeof(ruta_snapshot), "%s/snapshot.snar", DIR_RESPALDOS);

    char ruta_respaldo[300];
    snprintf(ruta_respaldo, sizeof(ruta_respaldo), "%s/respaldo_%s.tar.gz", DIR_RESPALDOS, marca_tiempo);

    /* --listed-incremental usa el archivo .snar como "memoria": si no existe,
     * tar hace un respaldo completo y lo crea; si ya existe, tar compara
     * metadatos y solo agrega al .tar.gz lo que cambió desde el respaldo
     * anterior (respaldo incremental real, no simulado). */
    char comando[900];
    snprintf(comando, sizeof(comando),
             "tar --listed-incremental=%s -czf %s -C %s . 2>/dev/null",
             ruta_snapshot, ruta_respaldo, directorio_origen);

    int estado = system(comando);
    return (estado == 0) ? 0 : -1;
}

int restaurar_version(const char *archivo_respaldo) {
    char carpeta_destino[300];
    snprintf(carpeta_destino, sizeof(carpeta_destino), "%s/restaurado", DIR_RESPALDOS);

    struct stat st;
    if (stat(carpeta_destino, &st) != 0) {
        mkdir(carpeta_destino, 0755);
    }

    char comando[700];
    snprintf(comando, sizeof(comando), "tar -xzf %s -C %s 2>/dev/null", archivo_respaldo, carpeta_destino);

    int estado = system(comando);
    return (estado == 0) ? 0 : -1;
}

int listar_respaldos(char lista[][256], int max_elementos) {
    asegurar_carpeta_respaldos();

    DIR *dir = opendir(DIR_RESPALDOS);
    if (!dir) return 0;

    int contador = 0;
    struct dirent *ent;

    while ((ent = readdir(dir)) != NULL && contador < max_elementos) {
        if (strstr(ent->d_name, ".tar.gz") != NULL) {
            snprintf(lista[contador], 256, "%s/%s", DIR_RESPALDOS, ent->d_name);
            contador++;
        }
    }

    closedir(dir);
    return contador;
}