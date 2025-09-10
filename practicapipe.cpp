#include <iostream>
#include <unistd.h>   // pipe(), fork(), read(), write()
#include <cstring>    // strlen()

int main() {
    int fd[2];  // fd[0] = lecutra, fd[1] = escritura

    if (pipe(fd) == -1) {
        perror("error en la creacion de la pipe");
        return 1;
    }

    pid_t pid = fork();

    if (pid == -1) {
        perror("error en el fork");
        return 1;
    }

    if (pid == 0) {
        // Hijo lee de la pipe
        close(fd[1]); // cierre escritura
        char buffer[100];
        int n = read(fd[0], buffer, sizeof(buffer)); // leer
        buffer[n] = '\0'; // terminar string en NULL
        std::cout << "Hijo leyo: " << buffer << std::endl;
        close(fd[0]);
    } else {
        // padre escribe en la pipe
        close(fd[0]); // cerrar lectura
        const char* msg = "wuaja i\0 idsj";
        write(fd[1], msg, strlen(msg));
        close(fd[1]);
    }

    return 0;
}
