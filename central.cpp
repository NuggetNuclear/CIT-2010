#include <iostream>
#include <unistd.h>   // pipe(), fork(), read(), write()
#include <cstring>    // strlen()
#include <sys/stat.h> // mkfifo()
#include <fcntl.h>    // open() y flags O_RDONLY, O_WRONLY

// Proceso central o tambien 'Orquestador' de la comunicacion entre procesos

int main() {
    const char* C2O = "/tmp/orch_c2o.fifo"; // cliente -> orquestador
    const char* O2C = "/tmp/orch_o2c.fifo"; // orquestador -> cliente

    // Pipes con nombre (FIFOs)

    mkfifo(C2O, 0666);
    mkfifo(O2C, 0666);

    std::cout << "[CENTRAL] Bienvenido al Chat Comunitario" << std::endl;

    int clientToOrch = open(C2O, O_RDONLY); // Cliente --> Orquestador
    int orchToClient = open(O2C, O_WRONLY); // Orquestador --> Cliente
 
}