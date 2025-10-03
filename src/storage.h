#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>
#include <string.h>

// Inicializar almacenamiento
int storage_init(const char *filename);

// Guardar un nuevo dato (POST)
int storage_add(const char *value);

// Obtener un dato por id (GET)
int storage_get(int id, char *out, size_t max_len);

// Actualizar un dato por id (PUT)
int storage_update(int id, const char *new_value);

// Eliminar un dato por id (DELETE)
int storage_delete(int id);

#endif
