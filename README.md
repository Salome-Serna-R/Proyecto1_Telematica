# Proyecto 1 Telemática

## Objetivo
En el presente trabajo queremos desarrollar una aplicación Cliente-Servidor que toma datos reales de un sensor y los alimenta a un servidor que hosteamos en la nube.
Adicionalmente, queríamos desarrollar nuestra propia implementación del protocolo CoAP con el fin de entender mejor como se comunican los dispositivos por la red.

## Requisitos
PENDIENTE

## Pasos de Compilación
El servidor se puede compilar con el Makefile ejecutando el comando "make" en una terminal.
En el caso que esto no funcione, el método clásico también funciona:
"gcc -o server src/server.c src/coap_packet.c src/storage.c -lpthread"

El cliente de consulta de Python se ejecuta desde la terminal con python o python3

En la carpeta sensor se encuentra el .ino que se debe cargar en el ESP32, adicional de un archivo json que recrea las conexiones en Wokwi de forma rápida.

