#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <time.h>
#include <errno.h>
#include <signal.h>

#include "storage.h"
#include "coap_packet.h"
#include "log.h"

// === CONSTANTES DEL SERVIDOR ===
#define DEFAULT_SERVER_PORT 5683
#define MAX_BUFFER_SIZE 1500
#define MAX_CONCURRENT_THREADS 100
#define THREAD_TIMEOUT_SECONDS 30
#define MAX_PAYLOAD_SIZE 100
#define MAX_URI_LENGTH 32

// === VARIABLES GLOBALES ===
static int activeThreadCount = 0;
static pthread_mutex_t threadCountMutex = PTHREAD_MUTEX_INITIALIZER;
static FILE* serverLogFile = NULL;

// === ESTRUCTURAS DE DATOS ===
typedef struct {
    int socketFd;
    struct sockaddr_in clientAddress;
    socklen_t clientAddressLength;
    uint8_t receivedBuffer[MAX_BUFFER_SIZE];
    size_t receivedBufferLength;
} thread_args_t;


// === FUNCIONES DE LOGGING ===
/**
 * Registra un mensaje en el log del servidor con timestamp
 * @param formatString Formato del mensaje (como printf)
 * @param ... Argumentos para el formato
 */
void logServerMessage(const char* formatString, ...) {
    va_list argumentList;
    va_start(argumentList, formatString);

    time_t currentTime = time(NULL);
    char timestampBuffer[32];
    strftime(timestampBuffer, sizeof(timestampBuffer), "%Y-%m-%d %H:%M:%S", localtime(&currentTime));
    
    // Escribir en consola para monitoreo en tiempo real
    log_text("[%s]", timestampBuffer);
    vprintf(formatString, argumentList);
    log_text("\n");

    // Escribir en archivo de log si está disponible
    if (serverLogFile) {
        fprintf(serverLogFile, "[%s]", timestampBuffer);
        vfprintf(serverLogFile, formatString, argumentList);
        fprintf(serverLogFile, "\n");
        fflush(serverLogFile); // Escritura inmediata para debugging
    } else {
        log_text("[ERROR] No se pudo escribir en el archivo de log del servidor.");
    }
    
    va_end(argumentList);
}

// Conseguir ID de option
int coap_get_uri_id(const coap_packet_t *pkt) {
    for (int i = 0; i < pkt->options_count; i++) {
        if (pkt->options[i].number == 11) { // Uri-Path
            // revisar si es número
            char buf[32];
            if (pkt->options[i].length < sizeof(buf)) {
                memcpy(buf, pkt->options[i].value, pkt->options[i].length);
                buf[pkt->options[i].length] = '\0';
                // intentar parsear como entero
                int id = atoi(buf);
                if (id > 0) return id;
            }
        }
    }
    return -1; // no encontrado
}

// === MANEJADORES DE REQUESTS ===
/**
 * Maneja requests GET para recuperar datos de sensores
 * @param request Paquete CoAP de request
 * @param response Paquete CoAP de response (se modifica)
 */
void handleGetRequest(const coap_packet_t* request, coap_packet_t* response) {
    if (!request || !response) {
        return;
    }
    
    int sensorId = extractSensorIdFromUri(request);
    if (sensorId < 0) {
        logServerMessage("[ERROR] GET: ID de sensor inválido");
        response->code = COAP_CODE_BAD_REQ;
        return;
    }

    char sensorData[128];
    int storageResult = storage_get(sensorId, sensorData, sizeof(sensorData));
    
    if (storageResult == 0) {
        response->code = COAP_CODE_CONTENT;
        response->payload = (uint8_t*) sensorData;
        response->payload_len = strlen(sensorData);
        logServerMessage("[INFO] GET: Datos recuperados para sensor ID %d", sensorId);
    } else if (storageResult == -2) {
        logServerMessage("[WARNING] GET: Sensor ID %d no encontrado", sensorId);
        response->code = COAP_CODE_BAD_REQ;
    } else {
        logServerMessage("[ERROR] GET: Error interno al recuperar sensor ID %d", sensorId);
        response->code = COAP_CODE_BAD_REQ;
    }

    // Configurar respuesta CoAP
    response->ver = 1;
    response->type = COAP_TYPE_ACK;
    response->message_id = request->message_id;
    response->token_len = request->token_len;
    memcpy(response->token, request->token, request->token_len);
}


/**
 * Maneja requests POST para agregar nuevos datos de sensores
 * @param request Paquete CoAP de request
 * @param response Paquete CoAP de response (se modifica)
 */
void handlePostRequest(const coap_packet_t* request, coap_packet_t* response) {
    if (!request || !response) {
        return;
    }
    
    if (request->payload && request->payload_len > 0) {
        // Validar tamaño del payload para prevenir overflow
        if (request->payload_len > MAX_PAYLOAD_SIZE) {
            logServerMessage("[ERROR] POST: Payload demasiado grande (%zu bytes)", request->payload_len);
            response->code = COAP_CODE_BAD_REQ;
            return;
        }
        
        char payloadBuffer[128];
        int formattedLength = snprintf(payloadBuffer, sizeof(payloadBuffer), "%.*s", 
                                      (int)request->payload_len, request->payload);
        
        if (formattedLength >= sizeof(payloadBuffer)) {
            logServerMessage("[ERROR] POST: Buffer overflow en payload");
            response->code = COAP_CODE_BAD_REQ;
            return;
        }
        
        int storageResult = storage_add(payloadBuffer);
        if (storageResult == 0) {
            logServerMessage("[INFO] POST: Datos de sensor agregados exitosamente");
            response->code = COAP_CODE_CREATED;
        } else {
            logServerMessage("[ERROR] POST: Error al agregar datos al storage");
            response->code = COAP_CODE_BAD_REQ;
        }
    } else {
        logServerMessage("[ERROR] POST: Payload vacío o inválido");
        response->code = COAP_CODE_BAD_REQ;
    }
    
    // Configurar respuesta CoAP
    response->ver = 1;
    response->type = (request->type == COAP_TYPE_NON) ? COAP_TYPE_NON : COAP_TYPE_ACK;
    response->message_id = request->message_id;
    response->token_len = request->token_len;
    memcpy(response->token, request->token, request->token_len);
    response->payload = NULL;
    response->payload_len = 0;
}

// Usa hilos para manejar multiples clientes - Thread-safe con límites
void *handle_client(void *arg) {
    thread_args_t *args = (thread_args_t*) arg;
    
    // Incrementar contador de threads activos de forma thread-safe
    pthread_mutex_lock(&threadCountMutex);
    activeThreadCount++;
    pthread_mutex_unlock(&threadCountMutex);

    coap_packet_t req, resp;
    int res = coap_parse(args->buffer, args->buffer_len, &req);
    if (res != 0) {
        log_text("[ERROR] Paquete inválido (código %d)", res);
        free(args);
        goto cleanup;
    }

    logServerMessage("[INFO] Mensaje CoAP recibido: Ver=%d Type=%d Code=%d MID=0x%04X",
                     requestPacket.ver, requestPacket.type, requestPacket.code, requestPacket.message_id);

    // Procesar request según el método CoAP
    switch (requestPacket.code) {
        case COAP_CODE_GET:
            handleGetRequest(&requestPacket, &responsePacket);
            break;
        case COAP_CODE_POST:
            handlePostRequest(&requestPacket, &responsePacket);
            break;
        case COAP_CODE_PUT:
            {
                int sensorId = extractSensorIdFromUri(&requestPacket);
                if (sensorId < 0) {
                    responsePacket.code = COAP_CODE_BAD_REQ;
                    break;
                }
                
                if (requestPacket.payload && requestPacket.payload_len > 0) {
                    char updateBuffer[128];
                    snprintf(updateBuffer, sizeof(updateBuffer), "%.*s", 
                             (int)requestPacket.payload_len, requestPacket.payload);
                    
                    if (storage_update(sensorId, updateBuffer) == 0) {
                        responsePacket.code = COAP_CODE_CHANGED;
                        logServerMessage("[INFO] PUT: Datos actualizados para sensor ID %d", sensorId);
                    } else {
                        responsePacket.code = COAP_CODE_BAD_REQ;
                    }
                } else {
                    responsePacket.code = COAP_CODE_BAD_REQ;
                }
                
                // Configurar respuesta CoAP
                responsePacket.ver = 1;
                responsePacket.type = (requestPacket.type == COAP_TYPE_NON) ? COAP_TYPE_NON : COAP_TYPE_ACK;
                responsePacket.message_id = requestPacket.message_id;
                responsePacket.token_len = requestPacket.token_len;
                memcpy(responsePacket.token, requestPacket.token, requestPacket.token_len);
                responsePacket.payload = NULL;
                responsePacket.payload_len = 0;
            }
            break;
        case COAP_CODE_DELETE:
            {
                int sensorId = extractSensorIdFromUri(&requestPacket);
                if (sensorId < 0) {
                    responsePacket.code = COAP_CODE_BAD_REQ;
                    break;
                }
                
                if (storage_delete(sensorId) == 0) {
                    responsePacket.code = COAP_CODE_DELETED;
                    logServerMessage("[INFO] DELETE: Sensor ID %d eliminado exitosamente", sensorId);
                } else {
                    responsePacket.code = COAP_CODE_BAD_REQ;
                }
                
                // Configurar respuesta CoAP
                responsePacket.ver = 1;
                responsePacket.type = (requestPacket.type == COAP_TYPE_NON) ? COAP_TYPE_NON : COAP_TYPE_ACK;
                responsePacket.message_id = requestPacket.message_id;
                responsePacket.token_len = requestPacket.token_len;
                memcpy(responsePacket.token, requestPacket.token, requestPacket.token_len);
                responsePacket.payload = NULL;
                responsePacket.payload_len = 0;
            }
            break;
        default:
            // Método CoAP no soportado
            logServerMessage("[WARNING] Método CoAP no soportado: %d", requestPacket.code);
            responsePacket.ver = 1;
            responsePacket.type = (requestPacket.type == COAP_TYPE_NON) ? COAP_TYPE_NON : COAP_TYPE_ACK;
            responsePacket.code = COAP_CODE_BAD_REQ;
            responsePacket.message_id = requestPacket.message_id;
            responsePacket.token_len = requestPacket.token_len;
            memcpy(responsePacket.token, requestPacket.token, requestPacket.token_len);
            responsePacket.payload = NULL;
            responsePacket.payload_len = 0;
            break;
    }

    // Serializar y enviar respuesta CoAP
    uint8_t responseBuffer[MAX_BUFFER_SIZE];
    size_t responseLength;
    
    if (coap_build(&responsePacket, responseBuffer, &responseLength, sizeof(responseBuffer)) == 0) {
        ssize_t bytesSent = sendto(clientArgs->socketFd, responseBuffer, responseLength, 0,
                                  (struct sockaddr*) &clientArgs->clientAddress, 
                                  clientArgs->clientAddressLength);
        if (bytesSent < 0) {
            logServerMessage("[ERROR] Error enviando respuesta al cliente: %s", strerror(errno));
        } else {
            logServerMessage("[DEBUG] Respuesta enviada: %zd bytes", bytesSent);
        }
    } else {
        logServerMessage("[ERROR] Error serializando respuesta CoAP");
    }

cleanup:
    // Decrementar contador de threads activos de forma thread-safe
    pthread_mutex_lock(&threadCountMutex);
    activeThreadCount--;
    pthread_mutex_unlock(&threadCountMutex);
    
    // Liberar memoria del cliente
    free(clientArgs);
    return NULL;
}

// === FUNCIÓN PRINCIPAL ===
/**
 * Función principal del servidor CoAP
 * @param argc Número de argumentos de línea de comandos
 * @param argv Array de argumentos (puerto, archivo de log)
 * @return 0 si exitoso, 1 si hay error
 */
int main(int argc, char* argv[]) {
    int serverPort = DEFAULT_SERVER_PORT;
    const char* logFilePath = "server.log";
    
    // Procesar argumentos de línea de comandos
    if (argc > 1) {
        serverPort = atoi(argv[1]);
        if (serverPort <= 0 || serverPort > 65535) {
            fprintf(stderr, "Error: Puerto inválido. Usando puerto por defecto %d\n", DEFAULT_SERVER_PORT);
            serverPort = DEFAULT_SERVER_PORT;
        }
    }
    
    if (argc > 2) {
        logFilePath = argv[2];
    }

    // Inicializar sistema de logging
    if (log_init(logFilePath) != 0) {
        perror("Error inicializando sistema de logging");
        exit(1);
    }

    // Abrir archivo de log del servidor
    serverLogFile = fopen(logFilePath, "a");
    if (!serverLogFile) {
        perror("Error abriendo archivo de log del servidor");
        exit(1);
    }

    // Crear socket UDP
    int serverSocket = socket(AF_INET, SOCK_DGRAM, 0);
    if (serverSocket < 0) {
        perror("Error creando socket del servidor");
        exit(1);
    }

    // Configurar dirección del servidor
    struct sockaddr_in serverAddress;
    memset(&serverAddress, 0, sizeof(serverAddress));
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(serverPort);

    // Vincular socket a la dirección del servidor
    if (bind(serverSocket, (struct sockaddr*) &serverAddress, sizeof(serverAddress)) < 0) {
        perror("Error vinculando socket del servidor");
        close(serverSocket);
        exit(1);
    }

    // Inicializar sistema de almacenamiento
    storage_init("data.json");

    logServerMessage("Servidor CoAP iniciado - Puerto: %d, Log: %s", serverPort, logFilePath);

    // === BUCLE PRINCIPAL DEL SERVIDOR ===
    while (1) {
        // Verificar límite de threads concurrentes
        pthread_mutex_lock(&threadCountMutex);
        if (activeThreadCount >= MAX_CONCURRENT_THREADS) {
            pthread_mutex_unlock(&threadCountMutex);
            logServerMessage("[WARNING] Límite de threads alcanzado (%d), esperando...", MAX_CONCURRENT_THREADS);
            sleep(1);
            continue;
        }
        pthread_mutex_unlock(&threadCountMutex);

        // Asignar memoria para argumentos del thread
        thread_args_t* clientArgs = malloc(sizeof(thread_args_t));
        if (!clientArgs) {
            logServerMessage("[ERROR] Error asignando memoria para thread de cliente");
            continue;
        }

        args->sock = sock;
        args->client_len = sizeof(args->client_addr);
        args->buffer_len = recvfrom(sock, args->buffer, MAX_BUF, 0,
                                    (struct sockaddr*) &args->client_addr,
                                    &args->client_len);

        if (args->buffer_len > 0) {
            pthread_t tid;
            int result = pthread_create(&tid, NULL, handle_client, args);
            if (result != 0) {
                log_text("[ERROR] Error creando thread: %s", strerror(result));
                free(args);
            } else {
                pthread_detach(clientThreadId); // Thread se auto-libera al terminar
            }
        } else if (clientArgs->receivedBufferLength < 0) {
            logServerMessage("[ERROR] Error recibiendo datos del cliente: %s", strerror(errno));
            free(clientArgs);
        } else {
            // receivedBufferLength == 0, cliente cerró conexión
            free(clientArgs);
        }
    }

    // Cerrar socket del servidor (nunca se alcanza en bucle infinito)
    close(serverSocket);
    return 0;
}
