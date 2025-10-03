#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "storage.h"


static int entry_count = 0;
static int next_id = 1;
static char filename[256];

// Nombre del archivo global
static char storage_file[256];

// Inicialización
int storage_init(const char *filename) {
    strncpy(storage_file, filename, sizeof(storage_file)-1);
    // Si el archivo no existe, crear con un array vacío
    FILE *archivo = fopen(storage_file, "r");
    if (!archivo) {
        archivo = fopen(storage_file, "w");
        if (!archivo) return -1;
        fprintf(archivo, "[]");
    }
    fclose(archivo);
    return 0;
}

// Función auxiliar: leer todo el archivo en memoria
static char *read_file() {
    FILE *archivo = fopen(storage_file, "r+");
    if (!archivo) return NULL;
    fseek(archivo, 0, SEEK_END);
    long len = ftell(archivo);
    rewind(archivo);

    char *buf = malloc(len+1);
    fread(buf, 1, len, archivo);
    buf[len] = '\0';
    fclose(archivo);
    return buf;
}

// Función auxiliar: sobrescribir archivo
static int write_file(const char *content) {
    FILE *archivo = fopen(storage_file, "w+");
    if (!archivo) return -1;
    fputs(content, archivo);
    fclose(archivo);
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
        data[strlen(data)-3] = '\0'; // quitar ']' (quita la línea vacía al final, quita el ] y después quita el newline)
        strcat(data, ",\n");    
        strcat(data,"\t");
        strcat(data, entry);
        strcat(data, "\n]");
    }

    int response = write_file(data);
    free(data);
    return response;
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

int storage_update(int id, const char *new_value) {
    char *data = read_file();
    if (!data) return -1;

    char key[32];
    snprintf(key, sizeof(key), "\"id\":%d", id);

    char *p = strstr(data, key);
    if (!p) {
        free(data);
        return -2; // no encontrado
    }

    // Buscar inicio de value
    char *val = strstr(p, "\"value\":\"");
    if (!val) {
        free(data);
        return -3;
    }
    val += 9; // mover después de "value":"

    char *end = strchr(val, '"');
    if (!end) {
        free(data);
        return -4;
    }

    // Construir nueva cadena
    char new_data[8192];
    size_t prefix_len = val - data;
    strncpy(new_data, data, prefix_len);
    new_data[prefix_len] = '\0';

    strcat(new_data, new_value);
    strcat(new_data, end);

    int response = write_file(new_data);
    free(data);
    return response;
}

// Eliminar una entrada
int storage_delete(int id) {
    char *data = read_file();
    if (!data) return -1;

    char key[32];
    snprintf(key, sizeof(key), "\"id\":%d", id);

    char *p = strstr(data, key);
    if (!p) {
        free(data);
        return -2; // no encontrado
    }

    // Buscar inicio del objeto '{'
    char *start = p;
    while (start > data && *start != '{') start--;

    // Buscar fin del objeto '}'
    char *end = strchr(p, '}');
    if (!end) {
        free(data);
        return -3;
    }
    end++; // incluir '}'

    // Construir nueva cadena
    char new_data[8192];
    size_t prefix_len = start - data;
    strncpy(new_data, data, prefix_len);
    new_data[prefix_len] = '\0';

    // Saltar objeto eliminado
    strcat(new_data, end);

    // Arreglar posibles comas extras
    for (int i = 0; new_data[i]; i++) {
        if (new_data[i] == ',' && new_data[i+1] == ']') {
            memmove(&new_data[i], &new_data[i+1], strlen(&new_data[i+1])+1);
        }
    }

    int res = write_file(new_data);
    free(data);
    return res;
}
