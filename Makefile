CC = gcc
CFLAGS = -Wall -Wextra -O2 -g
LDFLAGS = -lpthread -lsqlite3

SRC = server.c coap_packet.c
OBJ = $(SRC:.c=.o)

all: server test_coap

server: $(OBJ)
	$(CC) $(CFLAGS) -o server $(OBJ) $(LDFLAGS)

test_coap: test_coap.c coap_packet.c
	$(CC) $(CFLAGS) -o test_coap test_coap.c coap_packet.c

clean:
	rm -f *.o server test_coap
