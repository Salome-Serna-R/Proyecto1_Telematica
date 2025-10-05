import socket
import sys
import random

# Tipos de mensaje CoAP
COAP_TYPE_CON = 0
COAP_TYPE_NON = 1
COAP_TYPE_ACK = 2
COAP_TYPE_RST = 3

# Métodos y códigos
COAP_CODE_GET     = 1
# COAP_CODE_POST    = 2 # El cliente de consulta no realiza POSTs
COAP_CODE_PUT     = 3
COAP_CODE_DELETE  = 4

# Respuestas comunes
COAP_CODE_CONTENT = 69   # 2.05
COAP_CODE_CREATED = 65   # 2.01
COAP_CODE_CHANGED = 68   # 2.04
COAP_CODE_DELETED = 66   # 2.02
COAP_CODE_BAD_REQ = 128  # 4.00

# Construir paquete CoAP simple
def build_coap_packet(code, mid, uri_path=None, payload=None, msg_type = COAP_TYPE_CON):
    version = 1
    token = b''

    first_byte = (version << 6) | (msg_type << 4) | len(token)
    header = bytes([first_byte, code, (mid >> 8) & 0xFF, mid & 0xFF])
    packet = header + token

    # Opciones: Uri-Path (número=11)
    if uri_path:
        prev_opt_num = 0
        for segment in uri_path.split('/'):
            if not segment: continue
            opt_delta = 11 - prev_opt_num
            prev_opt_num = 11
            opt_len = len(segment)
            packet += bytes([ (opt_delta << 4) | opt_len ])
            packet += segment.encode()

    # Payload
    if payload:
        packet += bytes([0xFF]) + payload.encode()

    return packet


# Cliente principal
def main():
    if len(sys.argv) < 3:
        print("Uso: python3 client.py <server_ip> <GET|PUT|DELETE> <uri> [payload]")
        print("Ejemplo: python3 client.py 127.0.0.1 GET data/1")
        print("Ejemplo: python3 client.py 127.0.0.1 PUT data/1 \"25\"")
        print("Ejemplo: python3 client.py 127.0.0.1 DELETE data/1")
        print("RECORDATORIO: Este cliente es de consulta, no realiza la operacion POST.")
        sys.exit(1)

    server_ip = sys.argv[1]
    method = sys.argv[2].upper()
    uri = sys.argv[3]
    payload = sys.argv[4] if len(sys.argv) > 4 else None
    use_non = False
    if "--non" in sys.argv:
        use_non = True

    if method == "GET":
        code = COAP_CODE_GET
    elif method == "PUT":
        code = COAP_CODE_PUT
    elif method == "DELETE":
        code = COAP_CODE_DELETE
    else:
        print("Método no soportado")
        sys.exit(1)

    mid = random.randint(0, 65535)
    msg_type = COAP_TYPE_NON if use_non else COAP_TYPE_CON
    
    packet = build_coap_packet(code, mid, uri_path=uri, payload=payload, msg_type=msg_type)

    sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    sock.settimeout(3)

    server = (server_ip, 5683)
    sock.sendto(packet, server)

    try:
        data, _ = sock.recvfrom(1500)
        print("Respuesta cruda:", data)

        if len(data) >= 4:
            ver = (data[0] >> 6) & 0x03
            msg_type = (data[0] >> 4) & 0x03
            tkl = data[0] & 0x0F
            code_resp = data[1]
            mid_resp = (data[2] << 8) | data[3]
            print(f"Ver={ver} Type={msg_type} Code={code_resp} MID={mid_resp}")

            if 0xFF in data:
                i = data.index(0xFF)
                payload = data[i+1:].decode(errors="ignore")
                print("Payload:", payload)
    except socket.timeout:
        print("Tiempo de espera agotado, retransmitiendo...")
        data, _ = sock.recvfrom(1500)
        print("Respuesta cruda:", data)

        if len(data) >= 4:
            ver = (data[0] >> 6) & 0x03
            msg_type = (data[0] >> 4) & 0x03
            tkl = data[0] & 0x0F
            code_resp = data[1]
            mid_resp = (data[2] << 8) | data[3]
            print(f"Ver={ver} Type={msg_type} Code={code_resp} MID={mid_resp}")

            if 0xFF in data:
                i = data.index(0xFF)
                payload = data[i+1:].decode(errors="ignore")
                print("Payload:", payload)

    sock.close()

if __name__ == "__main__":
    main()
