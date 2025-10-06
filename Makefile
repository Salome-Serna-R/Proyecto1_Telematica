CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lpthread

SRC = server.c coap_packet.c storage.c log.c
OBJ = $(SRC:.c=.o)

server: $(OBJ)
	$(CC) $(CFLAGS) -o server $(OBJ) $(LDFLAGS)
	@echo "Compilación finalizada."

coap_packet.o: src/coap_packet.c include/coap_packet.h
	$(CC) $(CFLAGS) -c coap_packet.o src/coap_packet.c

storage.o: src/storage.c include/storage.h
	$(CC) $(CFLAGS) -c storage.o src/storage.c

server.o: src/server.c
	$(CC) $(CFLAGS) -c server.o src/server.c

log.o: src/log.c include/log.h
	$(CC) $(CFLAGS) -c log.o src/log.c

clean:
	rm -f *.o
	@echo "Eliminados archivos de objeto (.o)"

.PHONY: clean 