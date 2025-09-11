#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>

bool parse_pid_message(const std::string& input, pid_t& pid, std::string& message) {
    size_t open = input.find('[');
    if (open == std::string::npos) return false;

    size_t close = input.find(']', open + 1);  // primer ']' después de '['
    if (close == std::string::npos) return false;

    // Extraer PID
    std::string pid_str = input.substr(open + 1, close - open - 1);
    try {
        pid = static_cast<pid_t>(std::stoi(pid_str));
    } catch (...) {
        return false; // No era un número válido
    }

    // Buscar el separador "-"
    size_t dash = input.find('-', close + 1);
    if (dash == std::string::npos) return false;

    // Lo que está después del '-' es el mensaje
    message = input.substr(dash + 1);
    return true;
}

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
            pid_t pid;
            std::string msg;
            if (parse_pid_message(buffer, pid, msg)) {
                std::cout << "[" << pid << "] " << msg;
                (void)write(orchToClient, "OK", 2);
            } else {
                (void)write(orchToClient, "Error", 5);
            }
        } else if (bytesRead == 0) {
            std::cout << "Cliente desconectado" << std::endl;
            break;
        } else {
            std::cerr << "Error al leer el mensaje" << std::endl;
            break;
        }
    }
}