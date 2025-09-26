/* Include guard */
#ifndef COAP_PACKET_H
#define COAP_PACKET_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <stdio.h>

#define COAP_VER(b) (((b)[0] & 0xC0) >> 6)
#define COAP_TYPE(b) (((b)[0] & 0x30) >> 4)
#define COAP_TKL(b) ((b)[0] & 0x0F)
#define COAP_CODE(b) ((b)[1])
#define COAP_MID(b) (((uint16_t)(b)[2] << 8) | (b)[3])

// Definimos las opciones de un mensaje
typedef struct {
    uint16_t number;     // número de opción CoAP (ej. 11 = Uri-Path)
    uint16_t length;     // longitud del valor
    uint8_t  *value;     // puntero al valor
} coap_option_t;

// Definimos qué es un paquete de CoAP
typedef struct {
    uint8_t ver;
    uint8_t type;
    uint8_t tkl;
    uint8_t code;
    uint16_t message_id;
    uint8_t token[8];
    size_t token_len;
    coap_option_t options[16];
    size_t options_count;
    uint8_t *payload;
    size_t payload_len;
} coap_packet_t;

// Definimos los tipos de mensajes
typedef enum {
    COAP_TYPE_CON = 0,
    COAP_TYPE_NON = 1,
    COAP_TYPE_ACK = 2,
    COAP_TYPE_RST = 3,
} coap_type_t;

// Definimos los tipos de códigos que pueden tener los mensajes
typedef enum {
    COAP_CODE_EMPTY = 0x00,
    COAP_CODE_GET = 0x01,
    COAP_CODE_POST = 0x02,
    COAP_CODE_PUT = 0x03,
    COAP_CODE_DELETE = 0x04,
    // Códigos 2.xx
    COAP_CODE_CREATED = 65,
    COAP_CODE_DELETED = 66,
    COAP_CODE_VALID = 67,
    COAP_CODE_CHANGED = 68,
    COAP_CODE_CONTENT = 69,
    // Errores 4.xx
    COAP_CODE_BAD_REQ = 128
} coap_code_t;

int coap_parse(const uint8_t *buffer, size_t len, coap_packet_t *paquete);

int coap_build(const coap_packet_t *paquete, uint8_t *out_buffer, size_t *out_len, size_t max_len);

bool coap_validate(const coap_packet_t *paquete);

#endif