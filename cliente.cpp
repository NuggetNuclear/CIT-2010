// cliente.cpp
// Chat client process (independiente). Habla con el proceso central mediante FIFOs nombrados.
// Comentarios en español para explicar el flujo; código y nombres en inglés (camelCase).

#include <iostream>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstring>
#include <sys/select.h>
#include <signal.h>
#include <cerrno>
#include <sys/types.h>
#include <algorithm>  // std::max

// =============== Señales ===============
// Ctrl+C (SIGINT) debe cerrar de forma limpia enviando un "bye" al central.
static volatile sig_atomic_t stopRequested = 0;
static void handleSigint(int) { stopRequested = 1; }

// =============== Rutas y utilidades ===============
// Convención: C2O = client->orchestrator (subida al central); O2C = orchestrator->client (bajada a este cliente).
static const char* centralUpFifoPath() { return "/tmp/orch_c2o.fifo"; }

static std::string o2cPathFor(pid_t pid) {
    // Cada cliente escucha respuestas del central en su FIFO dedicado.
    return "/tmp/orch_o2c_" + std::to_string(pid) + ".fifo";
}

// Crea un FIFO si no existe; si ya existe (EEXIST), lo consideramos OK.
static bool ensureFifoExists(const char* path, mode_t mode = 0666) {
    if (mkfifo(path, mode) == 0) return true;
    return (errno == EEXIST);
}

// =============== Apertura de canales ===============
// Abrimos el FIFO O2C propio en RDWR para que no se cierre por falta de lectores/escritores.
static int openMyO2cRdwr(const std::string& path) {
    if (!ensureFifoExists(path.c_str())) {
        std::perror("[CLIENT] mkfifo(O2C)");
        return -1;
    }
    int fd = open(path.c_str(), O_RDWR);
    if (fd < 0) std::perror("[CLIENT] open(O2C O_RDWR)");
    return fd;
}

// Abrimos el FIFO C2O en modo escritura. Intentamos no bloqueante para evitar ENXIO si el central no está listo.
// Luego lo devolvemos a bloqueante.
static int openC2oWriter(const char* path) {
    if (!ensureFifoExists(path)) {
        std::perror("[CLIENT] mkfifo(C2O)");
        return -1;
    }
    int fd = -1;
    for (;;) {
        fd = open(path, O_WRONLY | O_NONBLOCK);
        if (fd >= 0) break;
        if (errno == ENXIO || errno == ENOENT) { usleep(200 * 1000); continue; }
        std::perror("[CLIENT] open(C2O O_WRONLY|NONBLOCK)");
        return -1;
    }
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    return fd;
}

// =============== Mensajería ===============
// Estos helpers envían marcas de conexión/desconexión y mensajes del usuario.
// Protocolo: "[PID]-texto\n"
static void sendConnectHello(int c2oFd, pid_t pid) {
    const std::string hello = "[" + std::to_string(pid) + "]-Proceso conectado\n";
    (void)write(c2oFd, hello.c_str(), hello.size());
}

static void sendDisconnectBye(int c2oFd, pid_t pid) {
    const std::string bye = "[" + std::to_string(pid) + "]-Proceso desconectado\n";
    (void)write(c2oFd, bye.c_str(), bye.size());
}

static bool sendUserMessage(int c2oFd, pid_t pid, const std::string& text) {
    const std::string payload = "[" + std::to_string(pid) + "]-" + text + "\n";
    ssize_t n = write(c2oFd, payload.c_str(), payload.size());
    if (n < 0) {
        if (errno == EPIPE) std::cerr << "[CLIENT] Central no disponible (EPIPE). Saliendo.\n";
        else std::perror("[CLIENT] write(C2O)");
        return false;
    }
    return true;
}

// =============== E/S entrante del central ===============
// Lee lo que el central envía por el FIFO O2C propio y lo muestra en pantalla.
static bool handleIncomingFromCentral(int o2cFd) {
    char buffer[4096];
    const ssize_t n = read(o2cFd, buffer, sizeof(buffer) - 1);
    if (n > 0) {
        buffer[n] = '\0';
        std::cout << "\r" << buffer;
        std::cout << "> " << std::flush;
        return true;
    }
    if (n == 0) {
        std::cout << "\n[CLIENT] Central cerró la conexión.\n";
    } else {
        std::perror("[CLIENT] read(O2C)");
    }
    return false;
}

// =============== E/S de usuario ===============
// Lee una línea desde stdin y la envía al central. Soporta comandos de salida.
static bool handleUserInput(int c2oFd, pid_t pid) {
    std::string line;
    if (!std::getline(std::cin, line)) {
        // EOF (Ctrl+D) → avisamos y salimos
        sendDisconnectBye(c2oFd, pid);
        return false;
    }

    if (line == "/leave" || line == "/xao") {
        sendDisconnectBye(c2oFd, pid);
        return false;
    }

    // TODO: implementar "/share" para duplicar este cliente (crear otro proceso que se conecte).
    //       Idea: fork() + execv(argv[0], ...) o re-ejecutar binario del cliente.
    // if (line.rfind("/share", 0) == 0) { /* TODO */ }

    // TODO: implementar "/report <pid>" para enviar "reportar <pid>" al central según el enunciado.
    // if (line.rfind("/report ", 0) == 0) { /* formatear "reportar <pid>" y enviar */ }

    return sendUserMessage(c2oFd, pid, line);
}

// =============== Bucle principal ===============
// Multiplexa stdin y FIFO O2C con select(). Mantiene el cliente vivo leyendo y enviando.
static void runChatLoop(int c2oFd, int o2cFd, pid_t pid) {
    std::cout << "[CLIENT] PID = " << pid << "\n";
    std::cout << "[CLIENT] Escribe y ENTER (Ctrl+D para salir; /leave o /xao también)\n";
    sendConnectHello(c2oFd, pid);
    std::cout << "> " << std::flush;

    for (;;) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(STDIN_FILENO, &rfds);
        FD_SET(o2cFd, &rfds);
        const int maxfd = std::max(STDIN_FILENO, o2cFd);

        const int ready = select(maxfd + 1, &rfds, nullptr, nullptr, nullptr);
        if (ready < 0) {
            if (errno == EINTR) {
                if (stopRequested) { sendDisconnectBye(c2oFd, pid); break; }
                continue;
            }
            std::perror("[CLIENT] select");
            break;
        }

        if (FD_ISSET(o2cFd, &rfds)) {
            if (!handleIncomingFromCentral(o2cFd)) break;
        }
        if (FD_ISSET(STDIN_FILENO, &rfds)) {
            if (!handleUserInput(c2oFd, pid)) break;
            std::cout << "> " << std::flush;
        }
    }
}

// =============== Programa principal ===============
// 1) Prepara señales. 2) Construye ruta O2C propia y abre RDWR. 3) Abre C2O para escribir.
// 4) Entra al loop de chat con select(). 5) Cierra recursos al terminar.
int main() {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handleSigint);

    const pid_t myPid = getpid();
    const std::string o2cPath = o2cPathFor(myPid);
    const char* c2oPath = centralUpFifoPath();

    const int o2cFd = openMyO2cRdwr(o2cPath);
    if (o2cFd < 0) return 1;

    const int c2oFd = openC2oWriter(c2oPath);
    if (c2oFd < 0) { close(o2cFd); return 1; }

    runChatLoop(c2oFd, o2cFd, myPid);

    close(c2oFd);
    close(o2cFd);
    return 0;
}