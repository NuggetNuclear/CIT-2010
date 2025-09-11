#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>

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
    char buffer[1024];

    while(true) {
        ssize_t bytesRead = read(clientToOrch, buffer, sizeof(buffer) - 1);

        if(bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::cout << buffer << std::endl;
            (void)write(orchToClient, "OK Reply", 8);
        } else if (bytesRead == 0) {
            std::cout << "Cliente desconectado" << std::endl;
            break;
        } else {
            std::cerr << "Error al leer el mensaje" << std::endl;
            break;
        }
    }
}