CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lpthread

SRC = server.c coap_packet.c storage.c
OBJ = $(SRC:.c=.o)

all: server

server: $(OBJ)
	$(CC) $(CFLAGS) -o server $(OBJ) $(LDFLAGS)

clean:
	rm -f *.o server
