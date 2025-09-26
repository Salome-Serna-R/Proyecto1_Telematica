#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
//#include <errno.h>

#include "storage.h"
#include "coap_packet.h"

// Puerto por defecto de CoAP
#define SERVER_PORT 5683
#define MAX_BUF 1500

// Estructura para pasar datos al thread
typedef struct {
    int sock;
    struct sockaddr_in client_addr;
    socklen_t client_len;
    uint8_t buffer[MAX_BUF];
    size_t buffer_len;
} thread_args_t;


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

void handle_get(coap_packet_t *request, coap_packet_t *response) {
    int id = coap_get_uri_id(request);
    if (id < 0) {
        response->code = COAP_CODE_BAD_REQ;
        return;
    }

    char value[128];
    if (storage_get(id, value, sizeof(value)) == 0) {
        response->code = COAP_CODE_CONTENT;
        response->payload = (uint8_t*) value;
        response->payload_len = strlen(value);
    } else {
        response->code = COAP_CODE_BAD_REQ;
    }

    response->ver = 1;
    response->type = COAP_TYPE_ACK;
    response->message_id = request->message_id;
    response->token_len = request->token_len;
    memcpy(response->token, request->token, request->token_len);
}


void handle_post(coap_packet_t *request, coap_packet_t *response) {
    if (request->payload && request->payload_len > 0) {
        char buf[128];
        snprintf(buf, sizeof(buf), "%.*s", (int)request->payload_len, request->payload);
        storage_add(buf);
    }

    printf("[INFO] POST recibido con payload: %.*s\n",
        (int)request->payload_len, request->payload);

    response->ver = 1;
    response->type = COAP_TYPE_ACK;
    response->code = COAP_CODE_CREATED;
    response->message_id = request->message_id;
    response->token_len = request->token_len;
    memcpy(response->token, request->token, request->token_len);
    response->payload = NULL;
    response->payload_len = 0;
}

// ---------- Thread handler ----------
void *handle_client(void *arg) {
    thread_args_t *args = (thread_args_t*) arg;

    coap_packet_t req, resp;
    int res = coap_parse(args->buffer, args->buffer_len, &req);
    if (res != 0) {
        printf("[ERROR] Paquete inválido (código %d)\n", res);
        free(args);
        return NULL;
    }

    printf("[INFO] Mensaje recibido: Ver=%d Type=%d Code=%d MID=0x%04X\n",
           req.ver, req.type, req.code, req.message_id);

    int uriId = 0; // Inicializar variable del Uri_ID del mensaje
    // Procesar según el código
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
                } else {
                    resp.code = COAP_CODE_BAD_REQ;
                }
            } else {
                resp.code = COAP_CODE_BAD_REQ;
            }
            resp.ver = 1;
            resp.type = COAP_TYPE_ACK;
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
            resp.type = COAP_TYPE_ACK;
            resp.message_id = req.message_id;
            resp.token_len = req.token_len;
            memcpy(resp.token, req.token, req.token_len);
            resp.payload = NULL;
            resp.payload_len = 0;
            break;
        default:
            // Respuesta vacía para códigos no implementados
            resp.ver = 1;
            resp.type = COAP_TYPE_ACK;
            resp.code = COAP_CODE_BAD_REQ;
            resp.message_id = req.message_id;
            resp.token_len = req.token_len;
            memcpy(resp.token, req.token, req.token_len);
            resp.payload = NULL;
            resp.payload_len = 0;
            break;
    }

    // Serializar respuesta
    uint8_t out[MAX_BUF];
    size_t out_len;
    if (coap_build(&resp, out, &out_len, sizeof(out)) == 0) {
        sendto(args->sock, out, out_len, 0,
               (struct sockaddr*) &args->client_addr, args->client_len);
    }

    free(args);
    return NULL;
}

// ---------- Main ----------
int main(int argc, char *argv[]) {
    int port = SERVER_PORT;
    if (argc > 1) {
        port = atoi(argv[1]);
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

    printf("Servidor CoAP escuchando en puerto %d...\n", port);

    while (1) {
        thread_args_t *args = malloc(sizeof(thread_args_t));
        if (!args) continue;

        args->sock = sock;
        args->client_len = sizeof(args->client_addr);
        args->buffer_len = recvfrom(sock, args->buffer, MAX_BUF, 0,
                                    (struct sockaddr*) &args->client_addr,
                                    &args->client_len);

        if (args->buffer_len > 0) {
            pthread_t tid;
            pthread_create(&tid, NULL, handle_client, args);
            pthread_detach(tid); // liberar thread al terminar
        } else {
            free(args);
        }
    }

    close(sock);
    return 0;
}
