CC = gcc
CFLAGS = -std=c11 -Wall -Wextra -O2
LDFLAGS = -pthread
TARGET = simulador
SRC = src/main.c src/config.c

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(SRC) -o $(TARGET) $(LDFLAGS)

run: $(TARGET)
	./$(TARGET) config.txt

clean:
	rm -f $(TARGET)
