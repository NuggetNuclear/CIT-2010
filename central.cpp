// central.cpp
// Proceso central (orchestrator). Gestiona logs y redifunde mensajes a todos los demás clientes.
// Comentarios en español para explicar el flujo; código y nombres en inglés (camelCase).

#include <iostream>
#include <unistd.h>
#include <cstring>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/types.h>
#include <signal.h>
#include <unordered_map>
#include <string>
#include <cerrno>
#include <sys/select.h>

// ========= Type alias =========
using FdByPid = std::unordered_map<pid_t, int>;

// ========= Signals =========
static volatile sig_atomic_t stopRequested = 0;
static void handleSigint(int){ stopRequested = 1; }

// ========= Parsing =========
// Formato de entrada desde clientes: "[PID]-mensaje"
bool parseMessage(const std::string& rawInput, pid_t& pidOut, std::string& messageOut) {
    std::string::size_type lb = rawInput.find('[');
    if (lb == std::string::npos) return false;

    std::string::size_type rb = rawInput.find(']', lb + 1);
    if (rb == std::string::npos || rb <= lb + 1) return false;

    std::string pidStr = rawInput.substr(lb + 1, rb - lb - 1);

    int parsedPid = 0;
    try {
        std::size_t consumed = 0;
        parsedPid = std::stoi(pidStr, &consumed);
        if (consumed != pidStr.size() || parsedPid <= 0) return false;
    } catch (...) { return false; }

    std::string::size_type dash = rawInput.find('-', rb + 1);
    if (dash == std::string::npos) return false;

    pidOut = static_cast<pid_t>(parsedPid);
    messageOut = rawInput.substr(dash + 1);
    return true;
}

// ========= FIFOs util =========
static bool ensureFifoExists(const char* path, mode_t mode = 0666) {
    int rv = mkfifo(path, mode);
    if (rv == 0) return true;
    if (errno == EEXIST) return true;
    std::perror("[CENTRAL] mkfifo");
    return false;
}

static std::string o2cPathFor(pid_t clientPid) {
    return "/tmp/orch_o2c_" + std::to_string(clientPid) + ".fifo";
}

static int openWriteToClient(pid_t clientPid) {
    const std::string path = o2cPathFor(clientPid);
    (void)ensureFifoExists(path.c_str());
    return open(path.c_str(), O_WRONLY | O_NONBLOCK);
}

// ========= Chat logic =========
// Envía un ACK inmediato al emisor para confirmar recepción.
static void sendAck(FdByPid& writerByPid, pid_t senderPid) {
    auto it = writerByPid.find(senderPid);
    if (it == writerByPid.end()) return;

    static const char ack[] = "[CENTRAL] ACK\n";
    ssize_t n = write(it->second, ack, sizeof(ack) - 1);
    if (n < 0) { close(it->second); writerByPid.erase(it); }
}

// Difunde el mensaje a todos los clientes excepto al emisor.
static void broadcastMessage(FdByPid& writerByPid, pid_t senderPid, const std::string& message) {
    std::string line = "[" + std::to_string(senderPid) + "] " + message + "\n";
    for (auto it = writerByPid.begin(); it != writerByPid.end();) {
        if (it->first == senderPid) { ++it; continue; }
        ssize_t n = write(it->second, line.c_str(), line.size());
        if (n < 0) { close(it->second); it = writerByPid.erase(it); }
        else { ++it; }
    }
}

// Procesa una línea completa: valida formato, asegura canal O2C del emisor, loggea, difunde y ACK.
static void processLine(const std::string& rawLine, FdByPid& writerByPid) {
    pid_t senderPid = 0;
    std::string message;

    if (!parseMessage(rawLine, senderPid, message)) {
        std::cerr << "[CENTRAL] Invalid line: " << rawLine << "\n";
        return;
    }

    if (!writerByPid.count(senderPid)) {
        int wfd = openWriteToClient(senderPid);
        if (wfd >= 0) {
            writerByPid[senderPid] = wfd;
            std::string welcome = "[CENTRAL] Conexion exitosa, tu PID es: " +
                                  std::to_string(senderPid) + "\n";
            (void)write(wfd, welcome.c_str(), welcome.size());
        } else {
            std::cerr << "[CENTRAL] No se pudo abrir O2C para PID " << senderPid << "\n";
        }
    }

    // Log visible del central
    std::cout << "[" << senderPid << "] " << message << "\n";

    // TODO: detección y conteo de reportes:
    //       si message empieza con "reportar <pid>", incrementar contador y, al llegar a 10,
    //       matar el proceso reportado (proceso anexo según enunciado). (kill(pid, SIGKILL))
    //       * Implementar como proceso anexo que recibe estos eventos del central o por FIFO dedicado. *

    // TODO: formateo enriquecido (hora/nick/pid) para los broadcasts.

    broadcastMessage(writerByPid, senderPid, message);
    sendAck(writerByPid, senderPid);
}

// Divide un chunk leído (puede traer varias líneas) y procesa línea a línea.
static void processChunk(const char* buffer, ssize_t bytesRead, FdByPid& writerByPid) {
    if (!buffer || bytesRead <= 0) return;
    std::string chunk(buffer, static_cast<size_t>(bytesRead));
    std::size_t pos = 0;
    while (true) {
        std::size_t nl = chunk.find('\n', pos);
        if (nl == std::string::npos) break;
        std::string line = chunk.substr(pos, nl - pos);
        pos = nl + 1;
        processLine(line, writerByPid);
    }
}

// ========= main (orchestrator) =========
// 1) Crea/abre C2O (lectura no bloqueante + escritor keep-alive).
// 2) Loop principal con select() sobre C2O.
// 3) Relee 0 bytes => reabre RDONLY|NONBLOCK (todos los escritores cerraron).
// 4) Propaga cada línea a los clientes conectados via O2C correspondiente.
int main() {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handleSigint);

    const char* c2oPath = "/tmp/orch_c2o.fifo";
    if (!ensureFifoExists(c2oPath)) return 1;

    int c2oRd = open(c2oPath, O_RDONLY | O_NONBLOCK);
    if (c2oRd < 0) { std::perror("[CENTRAL] open C2O RDONLY|NONBLOCK"); return 1; }

    int keepAlive = open(c2oPath, O_WRONLY | O_NONBLOCK);
    if (keepAlive < 0) { std::perror("[CENTRAL] open C2O keepAlive"); close(c2oRd); return 1; }

    FdByPid writerByPid;
    char buf[4096];

    while (!stopRequested) {
        fd_set rfds; FD_ZERO(&rfds); FD_SET(c2oRd, &rfds);
        int ready = select(c2oRd + 1, &rfds, nullptr, nullptr, nullptr);
        if (ready < 0) {
            if (errno == EINTR) continue;
            std::perror("[CENTRAL] select");
            break;
        }
        if (FD_ISSET(c2oRd, &rfds)) {
            ssize_t n = read(c2oRd, buf, sizeof(buf));
            if (n > 0) {
                processChunk(buf, n, writerByPid);
            } else if (n == 0) {
                // Todos los escritores cerraron -> reabrimos lector para esperar nuevos clientes
                close(c2oRd);
                c2oRd = open(c2oPath, O_RDONLY | O_NONBLOCK);
                if (c2oRd < 0) { std::perror("[CENTRAL] reopen C2O"); break; }
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
                std::perror("[CENTRAL] read C2O");
                break;
            }
        }
    }

    for (auto& kv : writerByPid) close(kv.second);
    close(keepAlive);
    close(c2oRd);
    return 0;
}