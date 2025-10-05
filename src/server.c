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

#define SERVER_PORT 5683   // Puerto por defecto de CoAP
#define MAX_BUF 1500
#define MAX_THREADS 100    // Límite de threads concurrentes
#define THREAD_TIMEOUT 30  // Timeout en segundos para threads

static int active_threads = 0;
static pthread_mutex_t thread_count_mutex = PTHREAD_MUTEX_INITIALIZER;
FILE *logfile = NULL;

// Estructura para pasar datos al thread
typedef struct {
    int sock;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    uint8_t buffer[MAX_BUF];
    size_t buffer_len;
} thread_args_t;

// Hacer log de los mensajes del servidor
void message_log(const char *fmt, ...) {
    va_list args;
    va_start(args, fmt);
    time_t now = time(NULL);
    char timestamp[32];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    log_text("[%s]", timestamp);
    vprintf(fmt, args);
    log_text("\n");

    if (logfile) {
        fprintf(logfile, "[%s]", timestamp);
        vfprintf(logfile, fmt, args);
        fprintf(logfile, "\n");
        fflush(logfile);
    } else {
        log_text("[ERROR] No se pudo escribir en el archivo de log.");
    }
    va_end(args);
}

// Conseguir ID de option - Con validación de memoria
int coap_get_uri_id(const coap_packet_t *pkt) {
    if (!pkt) return -1;

    if (pkt->options_count <= 0 || pkt->options_count > 16) {
        return -1;
    }

    for (int i = 0; i < pkt->options_count; i++) {
        if (pkt->options[i].number == 11) { // Uri-Path
            if (pkt->options[i].length > 0 && pkt->options[i].length < 32) {
                char buf[32];
                memcpy(buf, pkt->options[i].value, pkt->options[i].length);
                buf[pkt->options[i].length] = '\0';
                int id = atoi(buf);
                if (id > 0) return id;
            }
        }
    }
    return -1;
}

void handle_get(coap_packet_t *request, coap_packet_t *response) {
    if (!request || !response) return;

    int id = coap_get_uri_id(request);
    if (id < 0) {
        log_text("[ERROR] GET: ID inválido");
        response->code = COAP_CODE_BAD_REQ;
        return;
    }

    char value[128];
    int result = storage_get(id, value, sizeof(value));
    if (result == 0) {
        response->code = COAP_CODE_CONTENT;
        response->payload = (uint8_t*) value;
        response->payload_len = strlen(value);
        log_text("[INFO] GET: Datos recuperados para ID %d", id);
    } else if (result == -2) {
        log_text("[WARNING] GET: ID %d no encontrado", id);
        response->code = COAP_CODE_BAD_REQ;
    } else {
        log_text("[ERROR] GET: Error interno al recuperar ID %d", id);
        response->code = COAP_CODE_BAD_REQ;
    }

    response->ver = 1;
    response->type = (request->type == COAP_TYPE_NON) ? COAP_TYPE_NON : COAP_TYPE_ACK;
    response->message_id = request->message_id;
    response->token_len = request->token_len;
    memcpy(response->token, request->token, request->token_len);
}

void handle_post(coap_packet_t *request, coap_packet_t *response) {
    if (!request || !response) return;

    if (request->payload && request->payload_len > 0) {
        if (request->payload_len > 100) {
            log_text("[ERROR] POST: Payload demasiado grande (%zu bytes)", request->payload_len);
            response->code = COAP_CODE_BAD_REQ;
            return;
        }

        char buf[128];
        int len = snprintf(buf, sizeof(buf), "%.*s", (int)request->payload_len, request->payload);
        if (len >= sizeof(buf)) {
            log_text("[ERROR] POST: Buffer overflow en payload");
            response->code = COAP_CODE_BAD_REQ;
            return;
        }

        int result = storage_add(buf);
        if (result == 0) {
            log_text("[INFO] POST: Datos agregados exitosamente");
            response->code = COAP_CODE_CREATED;
        } else {
            log_text("[ERROR] POST: Error al agregar datos");
            response->code = COAP_CODE_BAD_REQ;
        }
    } else {
        log_text("[ERROR] POST: Payload vacío");
        response->code = COAP_CODE_BAD_REQ;
    }

    response->ver = 1;
    response->type = (request->type == COAP_TYPE_NON) ? COAP_TYPE_NON : COAP_TYPE_ACK;
    response->message_id = request->message_id;
    response->token_len = request->token_len;
    memcpy(response->token, request->token, request->token_len);
    response->payload = NULL;
    response->payload_len = 0;
}

// Usa hilos para manejar multiples clientes
void *handle_client(void *arg) {
    thread_args_t *args = (thread_args_t*) arg;
    if (!args) {
        log_text("[ERROR] Argumentos de thread nulos");
        return NULL;
    }

    pthread_mutex_lock(&thread_count_mutex);
    active_threads++;
    pthread_mutex_unlock(&thread_count_mutex);

    coap_packet_t req, resp;
    memset(&req, 0, sizeof(coap_packet_t));
    memset(&resp, 0, sizeof(coap_packet_t));

    int res = coap_parse(args->buffer, args->buffer_len, &req);
if (res != 0 || !coap_validate(&req)) {
    log_text("[ERROR] Paquete inválido, respondiendo con RST");

    coap_packet_t rst;
    memset(&rst, 0, sizeof(coap_packet_t));
    rst.ver = 1;
    rst.type = COAP_TYPE_RST;
    rst.code = COAP_CODE_EMPTY;
    rst.message_id = req.message_id; // eco del MID recibido

    uint8_t out[MAX_BUF];
    size_t out_len;
    if (coap_build(&rst, out, &out_len, sizeof(out)) == 0) {
        sendto(args->sock, out, out_len, 0,
               (struct sockaddr*) &args->client_addr, args->client_len);
    }
    goto cleanup;
}


    log_text("[INFO] Mensaje recibido: Ver=%d Type=%d Code=%d MID=0x%04X",
             req.ver, req.type, req.code, req.message_id);

    int uriId = 0;

    switch (req.code) {
        case COAP_CODE_GET:
            handle_get(&req, &resp);
            break;
        case COAP_CODE_POST:
            handle_post(&req, &resp);
            break;
        case COAP_CODE_PUT:
            uriId = coap_get_uri_id(&req);
            if (uriId < 0) {
                resp.code = COAP_CODE_BAD_REQ;
                break;
            }
            if (req.payload && req.payload_len > 0) {
                char buf[128];
                snprintf(buf, sizeof(buf), "%.*s", (int)req.payload_len, req.payload);
                if (storage_update(uriId, buf) == 0) {
                    resp.code = COAP_CODE_CHANGED;
                    log_text("[INFO] PUT recibido");
                } else {
                    resp.code = COAP_CODE_BAD_REQ;
                }
            } else {
                resp.code = COAP_CODE_BAD_REQ;
            }
            resp.ver = 1;
            resp.type = (req.type == COAP_TYPE_NON) ? COAP_TYPE_NON : COAP_TYPE_ACK;
            resp.message_id = req.message_id;
            resp.token_len = req.token_len;
            memcpy(resp.token, req.token, req.token_len);
            resp.payload = NULL;
            resp.payload_len = 0;
            break;
        case COAP_CODE_DELETE:
            uriId = coap_get_uri_id(&req);
            if (uriId < 0) {
                resp.code = COAP_CODE_BAD_REQ;
                break;
            }
            if (storage_delete(uriId) == 0) {
                resp.code = COAP_CODE_DELETED;
            } else {
                resp.code = COAP_CODE_BAD_REQ;
            }
            resp.ver = 1;
            resp.type = (req.type == COAP_TYPE_NON) ? COAP_TYPE_NON : COAP_TYPE_ACK;
            resp.message_id = req.message_id;
            resp.token_len = req.token_len;
            memcpy(resp.token, req.token, req.token_len);
            resp.payload = NULL;
            resp.payload_len = 0;
            break;
        default:
            resp.ver = 1;
            resp.type = (req.type == COAP_TYPE_NON) ? COAP_TYPE_NON : COAP_TYPE_ACK;
            resp.code = COAP_CODE_BAD_REQ;
            resp.message_id = req.message_id;
            resp.token_len = req.token_len;
            memcpy(resp.token, req.token, req.token_len);
            resp.payload = NULL;
            resp.payload_len = 0;
            break;
    }

    uint8_t out[MAX_BUF];
    size_t out_len;
    if (coap_build(&resp, out, &out_len, sizeof(out)) == 0) {
        ssize_t sent = sendto(args->sock, out, out_len, 0,
                             (struct sockaddr*) &args->client_addr, args->client_len);
        if (sent < 0) {
            log_text("[ERROR] Error enviando respuesta: %s", strerror(errno));
        }
    } else {
        log_text("[ERROR] Error serializando respuesta CoAP");
    }

cleanup:
    pthread_mutex_lock(&thread_count_mutex);
    active_threads--;
    pthread_mutex_unlock(&thread_count_mutex);
    free(args);
    return NULL;
}

int main(int argc, char *argv[]) {
    int port = SERVER_PORT;
    const char *logpath = "server.log";

    if (argc > 1) port = atoi(argv[1]);
    if (argc > 2) logpath = argv[2];

    if (log_init(logpath) != 0) {
        perror("log_init");
        exit(1);
    }

    logfile = fopen(logpath, "a");
    if (!logfile) {
        perror("fopen log");
        exit(1);
    }

    int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0) {
        perror("socket");
        exit(1);
    }

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(port);

    if (bind(sock, (struct sockaddr*) &servaddr, sizeof(servaddr)) < 0) {
        perror("bind");
        close(sock);
        exit(1);
    }

    storage_init("data.json");

    log_text("Servidor CoAP escuchando en el puerto %d, creando log en %s", port, logpath);

    while (1) {
        pthread_mutex_lock(&thread_count_mutex);
        if (active_threads >= MAX_THREADS) {
            pthread_mutex_unlock(&thread_count_mutex);
            log_text("[WARNING] Límite de threads alcanzado (%d), esperando...", MAX_THREADS);
            sleep(1);
            continue;
        }
        pthread_mutex_unlock(&thread_count_mutex);

        thread_args_t *args = malloc(sizeof(thread_args_t));
        if (!args) {
            log_text("[ERROR] Error asignando memoria para thread");
            continue;
        }

        memset(args, 0, sizeof(thread_args_t));
        args->sock = sock;
        args->client_len = sizeof(args->client_addr);

        args->buffer_len = recvfrom(sock, args->buffer, MAX_BUF, 0,
                                   (struct sockaddr*) &args->client_addr, &args->client_len);

        if (args->buffer_len > 0) {
            if (args->buffer_len > MAX_BUF) {
                log_text("[ERROR] Buffer demasiado grande: %zu bytes", args->buffer_len);
                free(args);
                continue;
            }

            pthread_t tid;
            int result = pthread_create(&tid, NULL, handle_client, args);
            if (result != 0) {
                log_text("[ERROR] Error creando thread: %s", strerror(result));
                free(args);
            } else {
                pthread_detach(tid);
            }
        } else if (args->buffer_len < 0) {
            log_text("[ERROR] Error recibiendo datos: %s", strerror(errno));
            free(args);
        } else {
            free(args);
        }
    }

    close(sock);
    return 0;
}
