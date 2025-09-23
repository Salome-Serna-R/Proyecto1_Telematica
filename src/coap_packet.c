#include "coap_packet.h"

// Macros que nos ayudaran más adelante :)
#define COAP_VER(b) (((b)[0] & 0xC0) >> 6)
#define COAP_TYPE(b) (((b)[0] & 0x30) >> 4)
#define COAP_TKL(b) ((b)[0] & 0x0F)
#define COAP_CODE(b) ((b)[1])
#define COAP_MID(b) (((uint16_t)(b)[2] << 8) | (b)[3])

// Validamos el paquete que se recibe
bool coap_validate(const coap_packet_t *paquete){
    if (paquete->ver != 0x01) return false;
    if (paquete->tkl > 8) return false;
    return true;
}

// Extraemos la información de los paquetes que nos llegan
int coap_parse(const uint8_t *buffer, size_t len, coap_packet_t *paquete){
    if (len < 4) return -1; // El mensaje es muy corto. Rechazar inmediatamente
    // Revisar el primer byte del mensaje (Versión, Tipo, y Longitud del Token)
    paquete->ver = COAP_VER(buffer);
    paquete->type = COAP_TYPE(buffer);
    paquete->tkl = COAP_TKL(buffer);
    // Extraer el código del paquete y el ID del mensaje
    paquete->code = COAP_CODE(buffer);
    paquete->message_id = COAP_MID(buffer);

    // Extraer el token
    if (paquete->tkl > 8 || 4 + paquete->tkl > len) return -2; // El token es inválido porque la longitud está mal configurada.
    paquete->token_len = paquete->tkl;
    memcpy(paquete->token, buffer + 4, paquete->token_len); // Copia los datos del buffer al token del paquete que estamos recibiendo.

    size_t index = 4 + paquete->token_len;
    // Buscar el payload del mensaje si hay (el marcador 0xFF)
    size_t payload_start = 0;
    for (size_t i = index; i<len; i++){
        if (buffer[i] == 0xFF){
            payload_start = i + 1;
            break;
        }
    }
    if (payload_start > 0 && payload_start < len){
        paquete->payload_len = len - payload_start;
        paquete->payload = (uint8_t*) (buffer + payload_start); // Un apuntador al buffer, no el original
    } else {
        paquete->payload = NULL;
        paquete->payload_len = 0;
    }
    return 0; // Si llegamos hasta aquí, es porque extraímos la información del mensaje sin problemas
}

int coap_build(const coap_packet_t *paquete, uint8_t *out_buffer, size_t *out_len, size_t max_len){
    size_t index = 0;
    if (paquete->token_len > 8) return -1; // Inmediatamente descartar un mensaje con una longitud de token inválida

    // Armar el primer byte (Versión, Tipo, y TKL)
    out_buffer[index++] = ((paquete->ver & 0x03) << 6) | ((paquete->type & 0x03) << 4) | (paquete->token_len & 0x0F);
    // Tomar los otros parámetros del mensaje.
    out_buffer[index++] = paquete->code;
    out_buffer[index++] = (paquete->message_id >> 8) & 0xFF;
    out_buffer[index++] = paquete->message_id & 0xFF;

    // Opciones del mensaje
    //TODO: Hacer eventualmente

    // Token
    memcpy(out_buffer + index, paquete->token, paquete->token_len);
    index += paquete->token_len;

    // Payload
    if (paquete->payload && paquete->payload_len > 0){
        if (index + 1 + paquete->payload_len > max_len) return -2; // El payload es más grande de lo que está permitido y se rechaza
        out_buffer[index++] = 0xFF; // Agregar el marcador del payload.
        memcpy(out_buffer + index, paquete->payload, paquete->payload_len);
        index += paquete->payload_len;
    }

    *out_len = index;
    return 0; // Mensaje construido con éxito
}
