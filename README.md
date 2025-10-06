# Proyecto 1 Telemática
Realizado por:

* Miguel Gómez
* Laura Ortiz
* Salome Serna

## Introducción
En el presente trabajo queremos desarrollar una aplicación Cliente-Servidor que toma datos reales de un sensor y los alimenta a un servidor que hosteamos en la nube.

Adicionalmente, queríamos desarrollar nuestra propia implementación del protocolo CoAP con el fin de entender mejor como se comunican los dispositivos por la red.

El programa fue desarrollado en C y Python.

## Desarrollo
A continuación se muestran especificaciones de ejecución y requisitos del proyecto.

El proyecto fue desarrollado principalmente en C, y pensado para manejar estructuras de datos altamente flexibles y livianas para asegurar su funcionamiento en todo tipo de dispositivos.

### Requisitos
Se requiere un computador con:

* Python instalado
* gcc instalado
* Un ESP32 (o uno simulado en Wokwi)
* Un DHT (o uno simulado en Wokwi)
* Conexión a Internet

### Pasos de Compilación
El servidor se puede compilar con el Makefile ejecutando el comando "make" en una terminal.

En el caso que esto no funcione, el método clásico también funciona:

`gcc -o server src/server.c src/coap_packet.c src/storage.c -lpthread`

El cliente de consulta de Python se ejecuta desde la terminal con python o python3.

Si se ejecuta sin parámetros, da un mensaje mostrando ejemplos de uso.

El cliente se debe ejecutar de la forma:

`python client.py <IP Servidor> <GET|PUT|DELETE> <uri> [payload]`

Ejemplo GET: `python client.py 127.0.0.1 GET data/1`

Ejemplo PUT: `python client.py 127.0.0.1 PUT data/1 "25"`

Adicionalmente, es posible mandar una petición con código NON al servidor de la forma:

`python client.py <IP Servidor> <GET|PUT|DELETE> <uri> [payload] --non`

Ejemplo: `python client.py 127.0.0.1 GET data/1 --non`

En la carpeta sensor se encuentra el .ino que se debe cargar en el ESP32, adicional de un archivo json que recrea las conexiones en Wokwi de forma rápida.

## Conclusiones
El proyecto permitió implementar desde cero un sistema IoT con CoAP, comprendiendo el ciclo completo de comunicación entre sensor, servidor y cliente. La integración en AWS evidenció la aplicabilidad real de la solución y la importancia de manejar la confiabilidad en protocolos sobre UDP. Más allá de la práctica técnica, se logró conectar teoría de protocolos con un caso funcional de IoT.

Este proyecto también nos permitió reafirmar la importancia de la documentación inicial en el desarrollo de un sistema. Desde la primera reunión pudimos identificar los requisitos, elaborar los primeros modelos y lograr una abstracción clara del proyecto, lo que nos facilitó comprender su alcance y contar con la información necesaria para iniciar el proceso de implementación de manera organizada y coherente.
Esto fue especialmente notorio porque se trataba de un proyecto muy diferente a los que normalmente habíamos desarrollado, lo que nos exigió adaptarnos a nuevas tecnologías, metodologías y conceptos propios del ámbito de las comunicaciones y los sistemas IoT.

Además, el proyecto reforzó la importancia de métodos de control de versión a la hora de crear proyectos, especialmente proyectos tan robustos como éste; al permitirnos volver atrás en nuestros pasos y encontrar errores a medida que íbamos desarrollando para poder identificar en qué exactamente se dio un error.

## Referencias
Google Drive con diagramas y referencias: https://drive.google.com/drive/folders/1cnLEIF7La9lvBdaENs_BYytbOq7G0s-V?usp=sharing

El video de sustentación se puede encontrar aquí: https://eafit-my.sharepoint.com/personal/ssernar2_eafit_edu_co/_layouts/15/stream.aspx?id=%2Fpersonal%2Fssernar2_eafit_edu_co%2FDocuments%2FGrabaciones%2FCall%20with%20Laura%20and%201%20other-20251005_194805-Meeting%20Recording%2FExports%2FVideoSustentacion_Telematica.mp4&ga=1&referrerScenario=AddressBarCopied.view.48812842-61dd-4ed8-b889-67825fa161f7&isDarkMode=true 

RFC de CoAP: https://datatracker.ietf.org/doc/html/rfc7252 

Documentación en un documento más presentable: https://docs.google.com/document/d/1nH8_rdsfjw0_gweKtVAswcOULd1cWyM5fpEY125MQsw/edit?usp=sharing 

Y, por supuesto, la clase de Internet: Arquitecturas y Protocolos de la Universidad EAFIT
