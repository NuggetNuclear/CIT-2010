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

    std::cout << "[CENTRAL] Iniciado - Esperando mensaje..." << std::endl;

    int clientToOrch = open(C2O, O_RDONLY); // Cliente --> Orquestador
    int orchToClient = open(O2C, O_WRONLY); // Orquestador --> Cliente

    char buffer[1024];
    while (true) {
        // Limpieza de buffer
        memset(buffer, 0, sizeof(buffer));
        
        // Leer mensaje del cliente
        ssize_t bytesRead = read(clientToOrch, buffer, sizeof(buffer) - 1);
        if (bytesRead > 0) {
            buffer[bytesRead] = '\0';
            std::cout << "[CENTRAL] Mensaje recibido: " << buffer << std::endl;
            std::string response = "Orquestador recibió: " + std::string(buffer);
            write(orchToClient, response.c_str(), response.size());
        } else if (bytesRead == 0) { // Cliente desconectado
            std::cout << "[CENTRAL] Cliente desconectado." << std::endl;
            break;
        } else { // Error de lectura
            std::cerr << "[CENTRAL] Error al leer del pipe." << std::endl;
            break;
        }
    }

    close(clientToOrch);
    close(orchToClient);

    unlink(C2O);
    unlink(O2C);

    return 0;
}