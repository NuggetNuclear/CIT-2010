#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>

int main() {
    const char* C2O = "/tmp/orch_c2o.fifo"; // cliente -> orquestador
    const char* O2C = "/tmp/orch_o2c.fifo"; // orquestador -> cliente

    mkfifo(C2O, 0666);
    mkfifo(O2C, 0666);

    int wfd = open(C2O, O_WRONLY);
    int rfd = open(O2C, O_RDONLY);

    pid_t clienteID = getpid();

    close(wfd);
    close(rfd);
    return 0;
}