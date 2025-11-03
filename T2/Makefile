CC = gcc
CFLAGS = -Wall -Wextra -pthread -g

BIN = simulador
SRC = src/main.c src/config.c

all: $(BIN)

$(BIN): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(BIN)

run: all
	@if [ -z "$(CONFIG)" ]; then \
		echo "Usando config.txt por defecto"; \
		./$(BIN); \
	else \
		echo "Usando $(CONFIG)"; \
		./$(BIN) "$(CONFIG)"; \
	fi

clean:
	rm -f $(BIN)
