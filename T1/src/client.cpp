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
#include <algorithm>
#include <cctype>

static volatile sig_atomic_t stopRequested = 0;
static void handleSigint(int) { stopRequested = 1; }

// ---------- Rutas ----------
static const char* getC2oPath() { return "/tmp/orch_c2o.fifo"; }
static std::string getO2cPathFor(pid_t pid) { return "/tmp/orch_o2c_" + std::to_string(pid) + ".fifo"; }

static bool ensureFifoExists(const char* path, mode_t mode = 0666) {
    if (mkfifo(path, mode) == 0) { return true; }
    if (errno == EEXIST) { return true; }
    std::perror("[CLIENTE] mkfifo");
    return false;
}

// ---------- Apertura de canales ----------
static int openMyDownlink(const std::string& o2cPath) {
    if (!ensureFifoExists(o2cPath.c_str())) {
        std::cerr << "[CLIENTE] No se pudo garantizar O2C\n";
        return -1;
    }

    int fd = open(o2cPath.c_str(), O_RDWR);
    if (fd < 0) {
        std::perror("[CLIENTE] open(O2C)");
        return -1;
    }
    return fd;
}

static int openUplinkWriter(const char* c2oPath) {
    if (!ensureFifoExists(c2oPath)) {
        std::cerr << "[CLIENTE] No se pudo garantizar C2O\n";
        return -1;
    }

    int fd = -1;
    while (true) {
        fd = open(c2oPath, O_WRONLY | O_NONBLOCK);
        if (fd >= 0) { break; }
        if (errno == ENXIO || errno == ENOENT) {
            usleep(200 * 1000);
            continue;
        }

        std::perror("[CLIENTE] open(C2O)");
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) {
        fcntl(fd, F_SETFL, flags & ~O_NONBLOCK);
    }
    return fd;
}

// ---------- Envío de mensajes  ----------
static void sendConnectHello(int c2oFd, pid_t pid) {
    std::string line = "[" + std::to_string(pid) + "]-Proceso conectado\n";
    (void)write(c2oFd, line.c_str(), line.size());
}

static void sendDisconnectBye(int c2oFd, pid_t pid) {
    std::string line = "[" + std::to_string(pid) + "]-Proceso desconectado\n";
    (void)write(c2oFd, line.c_str(), line.size());
}

static bool sendUserMessage(int c2oFd, pid_t pid, const std::string& text) {
    std::string payload = "[" + std::to_string(pid) + "]-" + text + "\n";
    ssize_t n = write(c2oFd, payload.c_str(), payload.size());
    if (n < 0) {
        if (errno == EPIPE) {
            std::cerr << "[CLIENTE] Central no disponible (EPIPE). Saliendo.\n";
        } else {
            std::perror("[CLIENTE] write(C2O)");
        }
        return false;
    }
    return true;
}

// ---------- Funciones de texto ----------
static bool startsWith(const std::string& s, const std::string& prefix) {
    if (s.size() < prefix.size()) { return false; }
    return s.compare(0, prefix.size(), prefix) == 0;
}

static std::string trim(const std::string& s) {
    size_t i = 0;
    size_t j = s.size();

    while (i < j && std::isspace(static_cast<unsigned char>(s[i])) != 0) { ++i; }
    while (j > i && std::isspace(static_cast<unsigned char>(s[j - 1])) != 0) { --j; }
    return s.substr(i, j - i);
}

// ---------- Recepcion desde central ----------
static bool handleIncomingFromCentral(int o2cFd) {
    char buffer[4096];
    ssize_t n = read(o2cFd, buffer, sizeof(buffer) - 1);

    if (n > 0) {
        buffer[n] = '\0';
        std::cout << "\r" << buffer;
        std::cout << "> " << std::flush;
        return true;
    }

    if (n == 0) {
        std::cout << "\n[CLIENTE] Central cerró la conexión.\n";
        return false;
    }

    std::perror("[CLIENTE] read(O2C)");
    return false;
}

// ---------- Entrada de usuario ----------
static bool handleUserInput(int c2oFd, pid_t pid, const std::string& programPath) {
    std::string line;

    if (!std::getline(std::cin, line)) {
        sendDisconnectBye(c2oFd, pid);
        return false;
    }

    if (line == "/leave") {
        sendDisconnectBye(c2oFd, pid);
        return false;
    }

    if (startsWith(line, "/share")) {
        pid_t child = fork();

        if (child == 0) {
            execlp(programPath.c_str(), programPath.c_str(), (char*)nullptr);
            std::perror("[CLIENTE] execlp(/share)");
            _exit(1);
        }

        if (child < 0) { std::perror("[CLIENTE] fork(/share)"); }
        return true;
    }

    if (startsWith(line, "/report")) {
        std::string rest = trim(line.substr(std::string("/report").size()));
        if (!rest.empty() && rest[0] == ' ') { rest.erase(0, 1); }

        try {
            long target = std::stol(rest);
            if (target > 0) {
                std::string payload = "reportar " + std::to_string(target);
                return sendUserMessage(c2oFd, pid, payload);
            }
        } catch (...) {
        }

        std::cout << "[CLIENTE] Uso correcto: /report <pid>\n";
        return true;
    }

    return sendUserMessage(c2oFd, pid, line);
}

// ---------- loop del chat ----------
static void runChatLoop(int c2oFd, int o2cFd, pid_t pid, const std::string& programPath) {
    std::cout << "[CLIENTE] PID = " << pid << "\n";
    std::cout << "[CLIENTE] Comandos: /leave | /share | /report <pid>\n";

    sendConnectHello(c2oFd, pid);
    std::cout << "> " << std::flush;

    while (true) {
        if (stopRequested != 0) {
            sendDisconnectBye(c2oFd, pid);
            break;
        }

        fd_set readFds;
        FD_ZERO(&readFds);
        FD_SET(STDIN_FILENO, &readFds);
        FD_SET(o2cFd, &readFds);

        int maxFd = std::max(STDIN_FILENO, o2cFd);
        int ready = select(maxFd + 1, &readFds, nullptr, nullptr, nullptr);

        if (ready < 0) {
            if (errno == EINTR) { continue; }

            std::perror("[CLIENTE] select");
            break;
        }

        if (FD_ISSET(o2cFd, &readFds) != 0) {
            if (!handleIncomingFromCentral(o2cFd)) { break; }
        }

        if (FD_ISSET(STDIN_FILENO, &readFds) != 0) {
            if (!handleUserInput(c2oFd, pid, programPath)) { break; }
            std::cout << "> " << std::flush;
        }
    }
}

int main(int argc, char** argv) {

    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handleSigint);

    pid_t myPid = getpid();
    std::string o2cPath = getO2cPathFor(myPid);
    const char* c2oPath = getC2oPath();

    int o2cFd = openMyDownlink(o2cPath);
    if (o2cFd < 0) { return 1; }

    int c2oFd = openUplinkWriter(c2oPath);
    if (c2oFd < 0) {
        close(o2cFd);
        return 1;
    }

    std::string programPath = (argc > 0 && argv && argv[0]) ? std::string(argv[0]) : "client";
    runChatLoop(c2oFd, o2cFd, myPid, programPath);

    close(c2oFd);
    close(o2cFd);
    return 0;
}