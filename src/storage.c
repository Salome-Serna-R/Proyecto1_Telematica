#include "storage.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// Nombre del archivo global
static char storage_file[256];

// Inicialización
int storage_init(const char *filename) {
    strncpy(storage_file, filename, sizeof(storage_file)-1);

    // Si el archivo no existe, crear con un array vacío
    FILE *f = fopen(storage_file, "r");
    if (!f) {
        f = fopen(storage_file, "w");
        if (!f) return -1;
        fprintf(f, "[]");
    }
    fclose(f);
    return 0;
}

// Función auxiliar: leer todo el archivo en memoria
static char *read_file() {
    FILE *f = fopen(storage_file, "r");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    rewind(f);

    char *buf = malloc(len+1);
    fread(buf, 1, len, f);
    buf[len] = '\0';
    fclose(f);
    return buf;
}

// Función auxiliar: sobrescribir archivo
static int write_file(const char *content) {
    FILE *f = fopen(storage_file, "w");
    if (!f) return -1;
    fputs(content, f);
    fclose(f);
    return 0;
}

// Generar timestamp ISO simple
static void get_timestamp(char *buf, size_t max) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, max, "%Y-%m-%dT%H:%M:%S", tm_info);
}

// Agregar un dato (POST)
int storage_add(const char *value) {
    char *data = read_file();
    if (!data) return -1;

    // Buscar último id
    int last_id = 0;
    char *p = data;
    while ((p = strstr(p, "\"id\":"))) {
        int id = atoi(p+5);
        if (id > last_id) last_id = id;
        p++;
    }

    int new_id = last_id + 1;
    char ts[64];
    get_timestamp(ts, sizeof(ts));

    // Crear nuevo objeto JSON
    char entry[256];
    snprintf(entry, sizeof(entry),
        "{\"id\":%d,\"ts\":\"%s\",\"value\":\"%s\"}",
        new_id, ts, value);

    // Insertar en el array
    if (strlen(data) <= 2) {
        // archivo vacío: []
        snprintf(data, 256, "[%s]", entry);
    } else {
        data[strlen(data)-1] = '\0'; // quitar ']'
        strcat(data, ",");
        strcat(data, entry);
        strcat(data, "]");
    }

    int res = write_file(data);
    free(data);
    return res;
}

// Obtener un valor por id
int storage_get(int id, char *out, size_t max_len) {
    char *data = read_file();
    if (!data) return -1;

    char key[16];
    snprintf(key, sizeof(key), "\"id\":%d", id);

    char *p = strstr(data, key);
    if (!p) {
        free(data);
        return -2; // no encontrado
    }

    // Buscar "value"
    char *val = strstr(p, "\"value\":\"");
    if (!val) {
        free(data);
        return -3;
    }
    val += 9;
    char *end = strchr(val, '"');
    if (!end) {
        free(data);
        return -4;
    }

    size_t len = end - val;
    if (len >= max_len) len = max_len - 1;
    strncpy(out, val, len);
    out[len] = '\0';

    free(data);
    return 0;
}

// TODO: implementar update y delete igual buscando "id":x
int storage_update(int id, const char *new_value) {
    // Ejercicio para Sprint 3: buscar id, reemplazar value y reescribir archivo
    return 0;
}

int storage_delete(int id) {
    // Ejercicio para Sprint 3: eliminar objeto con id dado del JSON
    return 0;
}

void storage_close() {
    // Nada especial para archivos planos
}
