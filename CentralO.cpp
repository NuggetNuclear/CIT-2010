#include <iostream>
#include <unistd.h>   // pipe(), fork(), read(), write()
#include <cstring>    // strlen()
#include <sys/stat.h> // mkfifo()
#include <fcntl.h>    // open() y flags O_READ, O_WRONLY

// Proceso central o tambien 'Orquestador' de la comunicacion entre procesos

int main() {
    const char* C2O = "/IPC/pipe";
    mkfifo(C2O, 0666); // creacion de la named pipe
    return 0;

    int clientToOrch = open(C2O, O_RDONLY ); // pipe del cliente al orquestador
    char buffer[100];
    read(clientToOrch, buffer, sizeof(buffer)); // lee el mensaje del cliente
    std::cout << "Mensaje recibido del cliente: " << buffer << std::endl;

    close(clientToOrch);
    unlink(C2O); // elimina la named pipe
    return 0;
}
