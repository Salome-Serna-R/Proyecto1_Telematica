//#include <stddef.h>
//#include <stdint.h>
#include "coap_packet.h"

#define COAP_VER(b) (((b)[0] & 0xC0) >> 6)
#define COAP_TYPE(b) (((b)[0] & 0x30) >> 4)
#define COAP_TKL(b) ((b)[0] & 0x0F)
#define COAP_CODE(b) ((b)[1])
#define COAP_MID(b) (((uint16_t)(b)[2] << 8) | (b)[3])

// Definimos la función de parseo
int coap_parse(const uint8_t *buf, size_t len, coap_packet_t *pkt){
    return 0;
}

int coap_build(const coap_packet_t *pkt, uint8_t *out, size_t *out_len){
    return 0;
}