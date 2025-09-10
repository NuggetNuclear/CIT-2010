#include <iostream>
#include <unistd.h>   // pipe(), fork(), read(), write()
#include <cstring>    // strlen()
#include <sys/stat.h> // mkfifo()
#include <fcntl.h>    // open() y flags O_RDONLY, O_WRONLY

// Proceso cliente que envia mensajes al orquestador

int main() {
    const char* C2O = "/tmp/orch_c2o.fifo"; // cliente -> orquestador
    mkfifo(C2O, 0666);
    std::cout << "cliente iniciado, enviando mensaje al orquestador" << std::endl;
    int wfd = open(C2O, O_WRONLY);
    const char* msg = "laFokinPrueba\n";
    write(wfd, msg, std::strlen(msg));
    close(wfd);
    return 0;
}
