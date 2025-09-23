#include "coap_packet.h"

int main() {
    // Simular un GET simple (sin payload)
    uint8_t raw[] = { 0x40, 0x01, 0x12, 0x34 }; // Ver=1, Type=CON, TKL=0, Code=GET, MID=0x1234
    coap_packet_t pkt;

    int res = coap_parse(raw, sizeof(raw), &pkt);
    if (res == 0) {
        printf("Parse OK: Ver=%d Type=%d Code=%d MID=%04X\n", pkt.ver, pkt.type, pkt.code, pkt.message_id);
    } else {
        printf("Parse ERROR %d\n", res);
    }

    // Construir respuesta con payload
    uint8_t out[128];
    size_t out_len;
    const char *payload = "Hola CoAP!";
    pkt.code = COAP_CODE_CONTENT;
    pkt.payload = (uint8_t*) payload;
    pkt.payload_len = strlen(payload);

    res = coap_build(&pkt, out, &out_len, sizeof(out));
    if (res == 0) {
        printf("Build OK, length=%zu\n", out_len);
    } else {
        printf("Build ERROR %d\n", res);
    }

    return 0;
}
