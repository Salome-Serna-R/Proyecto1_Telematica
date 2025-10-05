# Proyecto 1 Telemática
Realizado por:

* Miguel Gómez
* Laura Ortiz
* Salome Serna

## Introducción
En el presente trabajo queremos desarrollar una aplicación Cliente-Servidor que toma datos reales de un sensor y los alimenta a un servidor que hosteamos en la nube.

Adicionalmente, queríamos desarrollar nuestra propia implementación del protocolo CoAP con el fin de entender mejor como se comunican los dispositivos por la red.

El programa fue desarrollado en C y Python.

## Requisitos
Se requiere un computador con:

* Python instalado
* gcc instalado
* Un ESP32 (o uno simulado en Wokwi)
* Un DHT (o uno simulado en Wokwi)
* Conexión a Internet

## Pasos de Compilación
El servidor se puede compilar con el Makefile ejecutando el comando "make" en una terminal.

En el caso que esto no funcione, el método clásico también funciona:

`gcc -o server src/server.c src/coap_packet.c src/storage.c -lpthread`

El cliente de consulta de Python se ejecuta desde la terminal con python o python3.

Si se ejecuta sin parámetros, da un mensaje mostrando ejemplos de uso.

El cliente se debe ejecutar de la forma:

`python client.py <IP Servidor> <GET|PUT|DELETE> <uri> [payload]`

Ejemplo GET: `python client.py 127.0.0.1 GET data/1`

Ejemplo PUT: `python client.py 127.0.0.1 PUT data/1 "25"`

En la carpeta sensor se encuentra el .ino que se debe cargar en el ESP32, adicional de un archivo json que recrea las conexiones en Wokwi de forma rápida.

## Conclusiones
Este es un programa simple y ligero que es capaz de funcionar en dispositivos de muy escasos recursos, justo como es la intención del protcolo CoAP.

## Referencias
TODO
