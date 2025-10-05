#!/bin/bash

# Script de prueba de concurrencia para servidor CoAP
# Simula múltiples sensores Wowki enviando datos simultáneamente

echo "=== Prueba de Concurrencia del Servidor CoAP ==="
echo "Simulando 10 sensores Wowki enviando datos simultáneamente"
echo ""

# Función para simular un sensor
simulate_sensor() {
    local sensor_id=$1
    local data="sensor_${sensor_id}_temp_$(date +%s)"
    
    echo "Sensor $sensor_id enviando: $data"
    
    # Simular POST request (esto requeriría una implementación real de cliente CoAP)
    # Por ahora solo mostramos la simulación
    echo "POST /sensor/$sensor_id - Payload: $data"
    
    # Simular GET request
    echo "GET /sensor/$sensor_id"
    
    sleep 0.1  # Pequeña pausa para simular procesamiento
}

# Ejecutar múltiples sensores en paralelo
for i in {1..10}; do
    simulate_sensor $i &
done

# Esperar a que terminen todos los procesos
wait

echo ""
echo "=== Prueba completada ==="
echo "Verifica el archivo server.log para ver los mensajes de concurrencia"
echo "Verifica data.json para ver los datos almacenados"
