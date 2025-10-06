#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <pthread.h>
#include <errno.h>
#include "storage.h"

// Mutex para proteger operaciones de archivo
static pthread_mutex_t storage_mutex = PTHREAD_MUTEX_INITIALIZER;

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

// Función auxiliar: leer todo el archivo en memoria (thread-safe)
static char *read_file() {
    FILE *archivo = fopen(storage_file, "r");
    if (!archivo) return NULL;
    
    fseek(archivo, 0, SEEK_END);
    long len = ftell(archivo);
    rewind(archivo);

    char *buf = malloc(len+1);
    if (!buf) {
        fclose(archivo);
        return NULL;
    }
    
    size_t bytes_read = fread(buf, 1, len, archivo);
    buf[bytes_read] = '\0';
    fclose(archivo);
    return buf;
}

// Función auxiliar: sobrescribir archivo (thread-safe)
static int write_file(const char *content) {
    FILE *archivo = fopen(storage_file, "w");
    if (!archivo) return -1;
    
    size_t len = strlen(content);
    size_t written = fwrite(content, 1, len, archivo);
    fclose(archivo);
    
    return (written == len) ? 0 : -1;
}

// Generar timestamp ISO simple
static void get_timestamp(char *buf, size_t max) {
    time_t t = time(NULL);
    struct tm *tm_info = localtime(&t);
    strftime(buf, max, "%Y-%m-%dT%H:%M:%S", tm_info);
}

// Agregar un dato (POST) - Thread-safe
int storage_add(const char *value) {
    if (!value) return -1;
    
    pthread_mutex_lock(&storage_mutex);
    
    char *data = read_file();
    if (!data) {
        pthread_mutex_unlock(&storage_mutex);
        return -1;
    }

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

    // Crear nuevo objeto JSON con validación de tamaño
    char entry[512];
    int entry_len = snprintf(entry, sizeof(entry),
        "{\"id\":%d,\"ts\":\"%s\",\"value\":\"%s\"}",
        new_id, ts, value);
    
    if (entry_len >= sizeof(entry)) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -1; // Buffer overflow
    }

    // Construir nuevo JSON de forma segura
    size_t data_len = strlen(data);
    size_t new_len = data_len + entry_len + 10; // espacio extra para comas y brackets
    
    char *new_data = malloc(new_len);
    if (!new_data) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -1;
    }

    if (data_len <= 2) {
        // archivo vacío: []
        snprintf(new_data, new_len, "[%s]", entry);
    } else {
        data[data_len-1] = '\0'; // quitar ']'
        snprintf(new_data, new_len, "%s,%s]", data, entry);
    }

    int response = write_file(new_data);
    free(data);
    free(new_data);
    
    pthread_mutex_unlock(&storage_mutex);
    return response;
}

// Obtener un valor por id - Thread-safe
int storage_get(int id, char *out, size_t max_len) {
    if (!out || max_len == 0) return -1;
    
    pthread_mutex_lock(&storage_mutex);
    
    char *data = read_file();
    if (!data) {
        pthread_mutex_unlock(&storage_mutex);
        return -1;
    }

    char key[16];
    snprintf(key, sizeof(key), "\"id\":%d", id);

    char *p = strstr(data, key);
    if (!p) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -2; // no encontrado
    }

    // Buscar "value"
    char *val = strstr(p, "\"value\":\"");
    if (!val) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -3;
    }
    val += 9;
    char *end = strchr(val, '"');
    if (!end) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -4;
    }

    size_t len = end - val;
    if (len >= max_len) len = max_len - 1;
    strncpy(out, val, len);
    out[len] = '\0';

    free(data);
    pthread_mutex_unlock(&storage_mutex);
    return 0;
}

int storage_update(int id, const char *new_value) {
    if (!new_value) return -1;
    
    pthread_mutex_lock(&storage_mutex);
    
    char *data = read_file();
    if (!data) {
        pthread_mutex_unlock(&storage_mutex);
        return -1;
    }

    char key[32];
    snprintf(key, sizeof(key), "\"id\":%d", id);

    char *p = strstr(data, key);
    if (!p) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -2; // no encontrado
    }

    // Buscar inicio de value
    char *val = strstr(p, "\"value\":\"");
    if (!val) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -3;
    }
    val += 9; // mover después de "value":"

    char *end = strchr(val, '"');
    if (!end) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -4;
    }

    // Construir nueva cadena de forma segura
    size_t data_len = strlen(data);
    size_t new_value_len = strlen(new_value);
    size_t prefix_len = val - data;
    size_t suffix_len = strlen(end);
    size_t total_len = prefix_len + new_value_len + suffix_len + 1;
    
    char *new_data = malloc(total_len);
    if (!new_data) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -1;
    }
    
    strncpy(new_data, data, prefix_len);
    new_data[prefix_len] = '\0';
    strcat(new_data, new_value);
    strcat(new_data, end);

    int response = write_file(new_data);
    free(data);
    free(new_data);
    
    pthread_mutex_unlock(&storage_mutex);
    return response;
}

// Eliminar una entrada - Thread-safe
int storage_delete(int id) {
    pthread_mutex_lock(&storage_mutex);
    
    char *data = read_file();
    if (!data) {
        pthread_mutex_unlock(&storage_mutex);
        return -1;
    }

    char key[32];
    snprintf(key, sizeof(key), "\"id\":%d", id);

    char *p = strstr(data, key);
    if (!p) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -2; // no encontrado
    }

    // Buscar inicio del objeto '{'
    char *start = p;
    while (start > data && *start != '{') start--;

    // Buscar fin del objeto '}'
    char *end = strchr(p, '}');
    if (!end) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -3;
    }
    end++; // incluir '}'

    // Construir nueva cadena de forma segura
    size_t data_len = strlen(data);
    size_t prefix_len = start - data;
    size_t suffix_len = strlen(end);
    size_t total_len = prefix_len + suffix_len + 1;
    
    char *new_data = malloc(total_len);
    if (!new_data) {
        free(data);
        pthread_mutex_unlock(&storage_mutex);
        return -1;
    }
    
    strncpy(new_data, data, prefix_len);
    new_data[prefix_len] = '\0';
    strcat(new_data, end);

    // Arreglar posibles comas extras
    for (int i = 0; new_data[i]; i++) {
        if (new_data[i] == ',' && new_data[i+1] == ']') {
            memmove(&new_data[i], &new_data[i+1], strlen(&new_data[i+1])+1);
        }
    }

    int res = write_file(new_data);
    free(data);
    free(new_data);
    
    pthread_mutex_unlock(&storage_mutex);
    return res;
}
