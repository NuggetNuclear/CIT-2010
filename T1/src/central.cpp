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
#include <ctime>

using FdByPid = std::unordered_map<pid_t, int>;

static volatile sig_atomic_t stopRequested = 0;

static void handleSigint(int) { stopRequested = 1; }

// ---------- Rutas ----------
static constexpr const char* kC2oPath     = "/tmp/orch_c2o.fifo";      // clientes → central
static constexpr const char* kReportsPath = "/tmp/orch_reports.fifo";  // central  → moderator

// ---------- fifos ----------
static bool ensureFifoExists(const char* path, mode_t mode = 0666) {
    int rv = mkfifo(path, mode);
    if (rv == 0) { return true; }
    if (errno == EEXIST) { return true; }
    std::perror("[CENTRAL] mkfifo");
    return false;
}

static std::string getO2cPathFor(pid_t clientPid) { return "/tmp/orch_o2c_" + std::to_string(clientPid) + ".fifo"; }

static int openWriteToClient(pid_t clientPid) {
    std::string path = getO2cPathFor(clientPid);
    (void)ensureFifoExists(path.c_str());

    int fd = open(path.c_str(), O_WRONLY | O_NONBLOCK);
    if (fd < 0) { return -1; }
    return fd;
}

// ---------- Tiempo ----------
static std::string nowHms() {
    std::time_t t = std::time(nullptr);
    std::tm tm{};
    localtime_r(&t, &tm);
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d", tm.tm_hour, tm.tm_min, tm.tm_sec);
    return std::string(buf);
}

// ---------- PARSE ----------
static bool parseMessage(const std::string& raw, pid_t& pidOut, std::string& msgOut) {
    std::string::size_type lb = raw.find('[');
    if (lb == std::string::npos) { return false; }

    std::string::size_type rb = raw.find(']', lb + 1);
    if (rb == std::string::npos || rb <= lb + 1) { return false; }

    std::string pidStr = raw.substr(lb + 1, rb - lb - 1);

    int parsedPid = 0;
    try {
        std::size_t consumed = 0;
        parsedPid = std::stoi(pidStr, &consumed);

        if (consumed != pidStr.size()) {
            return false;
        }
        if (parsedPid <= 0) {
            return false;
        }
    } catch (...) {
        return false;
    }

    std::string::size_type dash = raw.find('-', rb + 1);
    if (dash == std::string::npos) {
        return false;
    }

    pidOut = static_cast<pid_t>(parsedPid);
    msgOut = raw.substr(dash + 1);
    return true;
}

// ---------- senders ----------
static void sendAck(FdByPid& writerByPid, pid_t senderPid) {
    auto it = writerByPid.find(senderPid);
    if (it == writerByPid.end()) { return; }

    std::string ack = "[CENTRAL " + nowHms() + "] ACK\n";
    ssize_t n = write(it->second, ack.c_str(), ack.size());

    if (n < 0) {
        close(it->second);
        writerByPid.erase(it);
    }
}

static void broadcastMessage(FdByPid& writerByPid, pid_t senderPid, const std::string& message) {
    std::string line = "[" + nowHms() + "][PID " + std::to_string(senderPid) + "] " + message + "\n";

    for (auto it = writerByPid.begin(); it != writerByPid.end();) {
        if (it->first == senderPid) {
            ++it;
            continue;
        }

        ssize_t n = write(it->second, line.c_str(), line.size());
        if (n < 0) {
            close(it->second);
            it = writerByPid.erase(it);
        } else {
            ++it;
        }
    }
}

// ---------- mod ----------
static pid_t spawnModerator() {
    if (!ensureFifoExists(kReportsPath)) {
        std::cerr << "[CENTRAL] No se pudo garantizar REPORTS_PATH\n";
        return -1;
    }

    pid_t child = fork();
    if (child < 0) {
        std::perror("[CENTRAL] fork(moderator)");
        return -1;
    }

    if (child == 0) {
        execlp("moderator", "moderator", (char*)nullptr);
        std::perror("[CENTRAL] execlp(moderator)");
        _exit(1);
    }

    return child;
}

static int openReportsWriter() {
    int fd = -1;

    while (true) {
        fd = open(kReportsPath, O_WRONLY | O_NONBLOCK);
        if (fd >= 0) { break; }

        if (errno == ENXIO || errno == ENOENT) {
            usleep(150 * 1000);
            continue;
        }

        std::perror("[CENTRAL] open(reports WR)");
        return -1;
    }

    int flags = fcntl(fd, F_GETFL, 0);
    if (flags != -1) { fcntl(fd, F_SETFL, flags & ~O_NONBLOCK); }
    return fd;
}

static bool handleReportIfAny(const std::string& message, int reportsFd) {
    const std::string key = "reportar ";
    if (message.rfind(key, 0) != 0) { return false; }

    std::string pidStr = message.substr(key.size());

    try {
        long target = std::stol(pidStr);
        if (target <= 0) { return true; }

        std::string line = std::to_string(target) + "\n";
        if (reportsFd >= 0) {
            ssize_t n = write(reportsFd, line.c_str(), line.size());
            if (n < 0) { std::perror("[CENTRAL] write(report)"); }
        }
    } catch (...) {
        // ignorar
    }

    return true;
}

// ---------- PROCESAMIENTO ----------
static void processLine(const std::string& rawLine, FdByPid& writerByPid, int reportsFd) {
    pid_t senderPid = 0;
    std::string message;

    if (!parseMessage(rawLine, senderPid, message)) {
        std::cerr << "[CENTRAL] Línea inválida: " << rawLine << "\n";
        return;
    }

    if (writerByPid.count(senderPid) == 0u) {
        int wfd = openWriteToClient(senderPid);
        if (wfd >= 0) {
            writerByPid[senderPid] = wfd;

            std::string welcome = "[CENTRAL " + nowHms() + "] Conexión OK. Tu PID: " + std::to_string(senderPid) + "\n";
            (void)write(wfd, welcome.c_str(), welcome.size());
        } else {
            std::cerr << "[CENTRAL] No se pudo abrir O2C para PID " << senderPid << "\n";
        }
    }

    std::cout << "[" << nowHms() << "][" << senderPid << "] " << message << "\n";

    if (handleReportIfAny(message, reportsFd)) {
        sendAck(writerByPid, senderPid);
        return;
    }

    broadcastMessage(writerByPid, senderPid, message);
    sendAck(writerByPid, senderPid);
}

static void processChunk(const char* buffer, ssize_t bytesRead, FdByPid& writerByPid, int reportsFd) {
    if (buffer == nullptr) { return; }
    if (bytesRead <= 0) { return; }

    std::string chunk(buffer, static_cast<size_t>(bytesRead));
    std::size_t pos = 0;

    while (true) {
        std::size_t nl = chunk.find('\n', pos);
        if (nl == std::string::npos) { break; }

        std::string line = chunk.substr(pos, nl - pos);
        pos = nl + 1;

        processLine(line, writerByPid, reportsFd);
    }
}

// ---------- main ----------
int main() {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGINT, handleSigint);

    if (!ensureFifoExists(kC2oPath)) { return 1; }
    if (!ensureFifoExists(kReportsPath)) { return 1; }

    pid_t modPid = spawnModerator();
    if (modPid <= 0) { std::cerr << "[CENTRAL] WARNING: moderador no iniciado.\n"; }

    int reportsFd = openReportsWriter();

    int c2oRd = open(kC2oPath, O_RDONLY | O_NONBLOCK);
    if (c2oRd < 0) {
        std::perror("[CENTRAL] open(C2O RDONLY|NONBLOCK)");
        return 1;
    }

    int keepAlive = open(kC2oPath, O_WRONLY | O_NONBLOCK);
    if (keepAlive < 0) {
        std::perror("[CENTRAL] open(C2O keepAlive)");
        close(c2oRd);
        return 1;
    }

    FdByPid writerByPid;
    char buf[4096];

    while (stopRequested == 0) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(c2oRd, &rfds);

        int ready = select(c2oRd + 1, &rfds, nullptr, nullptr, nullptr);

        if (ready < 0) {
            if (errno == EINTR) { continue; }
            std::perror("[CENTRAL] select");
            break;
        }

        if (FD_ISSET(c2oRd, &rfds) != 0) {
            ssize_t n = read(c2oRd, buf, sizeof(buf));
            if (n > 0) { 
                processChunk(buf, n, writerByPid, reportsFd);
            } else if (n == 0) {
                close(c2oRd);
                c2oRd = open(kC2oPath, O_RDONLY | O_NONBLOCK);
                if (c2oRd < 0) {
                    std::perror("[CENTRAL] reopen(C2O)");
                    break;
                }
            } else {
                if (errno == EAGAIN || errno == EWOULDBLOCK) {
                    continue;
                }
                std::perror("[CENTRAL] read(C2O)");
                break;
            }
        }
    }

    // FULL CLEANING 😈
    for (auto& kv : writerByPid) { close(kv.second); }
    if (reportsFd >= 0) { close(reportsFd); }
    close(keepAlive);
    close(c2oRd);

    if (modPid > 0) { kill(modPid, SIGTERM); }
    return 0;
}