#ifndef SERVER_CONFIG_H
#define SERVER_CONFIG_H

// Configuración del servidor CoAP para sensores Wowki
#define SERVER_PORT 5683
#define MAX_BUF 1500
#define MAX_THREADS 100
#define THREAD_TIMEOUT 30

// Límites de seguridad
#define MAX_PAYLOAD_SIZE 100
#define MAX_URI_LENGTH 32
#define MAX_VALUE_LENGTH 128

// Timeouts y reintentos
#define SOCKET_TIMEOUT 5
#define MAX_RETRIES 3

// Configuración de logging
#define LOG_LEVEL_INFO 1
#define LOG_LEVEL_WARNING 2
#define LOG_LEVEL_ERROR 3

#endif
