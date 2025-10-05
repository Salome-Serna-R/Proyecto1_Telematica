#!/bin/bash

# Script de prueba de memoria para detectar corrupción
echo "=== Prueba de Memoria del Servidor CoAP ==="
echo "Usando Valgrind para detectar problemas de memoria"
echo ""

# Función para limpiar procesos previos
cleanup() {
    echo "Limpiando procesos..."
    pkill -f "./server" 2>/dev/null
    sleep 1
}

# Función para probar con Valgrind
test_with_valgrind() {
    echo "Ejecutando servidor con Valgrind..."
    timeout 10s valgrind --tool=memcheck --leak-check=full --show-leak-kinds=all --track-origins=yes ./server 2>&1 | head -50
}

# Función para probar con AddressSanitizer
test_with_asan() {
    echo "Compilando con AddressSanitizer..."
    make clean
    gcc -Wall -Wextra -O2 -g -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -c src/server.c -o server_asan.o
    gcc -Wall -Wextra -O2 -g -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -c src/coap_packet.c -o coap_packet_asan.o
    gcc -Wall -Wextra -O2 -g -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -c src/storage.c -o storage_asan.o
    gcc -Wall -Wextra -O2 -g -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer -c src/log.c -o log_asan.o
    gcc -fsanitize=address -fsanitize=undefined -o server_asan server_asan.o coap_packet_asan.o storage_asan.o log_asan.o -lpthread
    
    echo "Ejecutando servidor con AddressSanitizer..."
    timeout 10s ./server_asan 2>&1 | head -30
}

# Limpiar procesos previos
cleanup

# Verificar si Valgrind está disponible
if command -v valgrind &> /dev/null; then
    test_with_valgrind
else
    echo "Valgrind no está disponible, usando AddressSanitizer..."
    test_with_asan
fi

echo ""
echo "=== Prueba completada ==="
echo "Revisa los mensajes de error arriba para detectar problemas de memoria"
