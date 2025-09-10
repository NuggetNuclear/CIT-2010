#include <iostream>
#include <unistd.h>   // pipe(), fork(), read(), write()
#include <cstring>    // strlen()
#include <sys/stat.h> // mkfifo()
#include <fcntl.h>    // open() y flags O_RDONLY, O_WRONLY

// Proceso central o tambien 'Orquestador' de la comunicacion entre procesos

int main() {
    const char* C2O = "/tmp/orch_c2o.fifo"; // cliente -> orquestador
    mkfifo(C2O, 0666); // creacion de la named pipe
    std::cout << "orquestador iniciado, esperando mensajes del cliente" << std::endl;
    int clientToOrch = open(C2O, O_RDONLY ); // pipe del cliente al orquestador
    char buffer[100];
    ssize_t n = read(clientToOrch, buffer, sizeof(buffer)); // lee el mensaje del cliente, size n para que sea seguro
    if (n > 0) {
        buffer[n] = '\0'; // el buffer es una cadena
        std::cout << "Mensaje recibido del cliente: " << buffer << std::endl;
    }

    close(clientToOrch);
    unlink(C2O); // elimina la named pipe
    return 0;
}
