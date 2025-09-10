#include <iostream>
#include <unistd.h>   // pipe(), fork(), read(), write()
#include <cstring>    // strlen()
#include <sys/stat.h> // mkfifo()
#include <fcntl.h>    // open() y flags O_RDONLY, O_WRONLY

// Proceso cliente que envia mensajes al orquestador

int main() {
    const char* C2O = "/tmp/orch_c2o.fifo"; // cliente -> orquestador
    const char* O2C = "/tmp/orch_o2c.fifo"; // orquestador -> cliente

    mkfifo(C2O, 0666);
    mkfifo(O2C, 0666);

    std::cout << "[CLIENTE] Conectando...\n";

    int wfd = open(C2O, O_WRONLY);
    int rfd = open(O2C, O_RDONLY);

    const char* msg = "fokin pipe\n";
    (void)write(wfd, msg, std::strlen(msg)); // simple write

    char buf[1024];
    ssize_t n = read(rfd, buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        std::cout << "[CLIENTE] respuesta: " << buf;
    } else if (n == 0) {
        std::cout << "[CLIENTE] central cerró la conexión.\n";
    } else {
        std::cerr << "[CLIENTE] error de lectura.\n";
    }

    close(wfd);
    close(rfd);
    return 0;
}