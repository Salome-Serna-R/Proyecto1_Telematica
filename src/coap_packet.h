/* Include guard */
#ifndef COAP_PACKET_H
#define COAP_PACKET_H

#include <stddef.h>
#include <stdint.h>
typedef struct {
    uint8_t ver;
    uint8_t type;
    uint8_t tkl;
    uint8_t code;
    uint16_t message_id;
    uint8_t token[8];
    size_t token_len;
    uint8_t *payload;
    size_t payload_len;
} coap_packet_t;

int coap_parse(const uint8_t *buf, size_t len, coap_packet_t *pkt);

int coap_build(const coap_packet_t *pkt, uint8_t *out, size_t *out_len);

#endif