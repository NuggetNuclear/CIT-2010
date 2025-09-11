#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>

int main() {
    const char* C2O = "/tmp/orch_c2o.fifo"; // cliente -> orquestador
    const char* O2C = "/tmp/orch_o2c.fifo"; // orquestador -> cliente

    // En un diseño típico crea el central; aquí no molesta reintentar
    mkfifo(C2O, 0666);
    mkfifo(O2C, 0666);

    // Apertura complementaria
    int c2o = open(C2O, O_WRONLY); // escribir a CENTRAL
    int o2c = open(O2C, O_RDONLY); // leer de CENTRAL

    pid_t clienteID = getpid();
    std::cout << "[CLIENTE] PID = " << clienteID << "\n";
    std::cout << "[CLIENTE] Escribe y ENTER (Ctrl+D para salir)\n";

    // Enviar un saludo inicial
    {
        std::string conectionMsg = "Proceso " + std::to_string(clienteID) + " se conectó al chat\n";
        (void)write(c2o, conectionMsg.c_str(), conectionMsg.size());
    }

    std::string line;
    char buffer[1024];

    while (true) {
        std::cout << "> ";
        if (!std::getline(std::cin, line)) {
            std::string leaveMsg = "Proceso " + std::to_string(clienteID) + " se desconectó\n";
            (void)write(c2o, leaveMsg.c_str(), leaveMsg.size());
            break;
        }

        std::string payload = "[" + std::to_string(clienteID) + "] " + line + "\n";
        (void)write(c2o, payload.c_str(), payload.size());

        ssize_t n = read(o2c, buffer, sizeof(buffer) - 1);
        if (n > 0) {
            buffer[n] = '\0';
            std::cout << "[CLIENTE] Respuesta: " << buffer << std::endl;
        } else if (n == 0) {
            std::cout << "[CLIENTE] Central cerró la conexión.\n";
            break;
        } else {
            std::cerr << "[CLIENTE] Error al leer respuesta.\n";
            break;
        }
    }

    close(c2o);
    close(o2c);
    return 0;
}
