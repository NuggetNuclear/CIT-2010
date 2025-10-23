CC = gcc
CFLAGS = -Wall -pthread -g

all:
	$(CC) $(CFLAGS) src/main.c src/config.c -o simulador

run: all
	./simulador

clean:
	rm -f simulador
