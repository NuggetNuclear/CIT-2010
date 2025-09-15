#include <iostream>
#include <unordered_map>
#include <string>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <csignal>
#include <cerrno>

static constexpr const char *kReportsPath = "/tmp/orch_reports.fifo"; // ruta inmutable de la FIFO 
static volatile sig_atomic_t stopRequested = 0; // señal para terminar ordenadamente
static void handleSigint(int) { stopRequested = 1; } // manejar SIGINT

static bool ensureFifoExists(const char *path, mode_t mode = 0666) {
    int returnValue = mkfifo(path, mode);
    if (returnValue == 0) { return true; }
    if (errno == EEXIST) { return true; }
    std::perror("[MOD] mkfifo");
    return false;
}

int main() {
    signal(SIGPIPE, SIG_IGN); // ignorar SIGPIPE
    signal(SIGINT, handleSigint); // manejar SIGINT

    if (!ensureFifoExists(kReportsPath)) { return 1; } // asegurar que la FIFO existe

    int rd = open(kReportsPath, O_RDONLY); // abre el exttremo de lectura de la FIFO
    if (rd < 0) {
        std::perror("[MOD] open(reports RD)");
        return 1;
    }

    int keepAlive = open(kReportsPath, O_WRONLY | O_NONBLOCK); // abre el extremo de escritura de la fifo, SOLO para mantenerla viva
    if (keepAlive < 0) { std::perror("[MOD] open(reports keepAlive)"); } // 

    std::unordered_map<pid_t, int> countByPid; // contador de reportes por PID
    std::string carry;
    char buf[512];

    while (stopRequested == 0) {
        ssize_t n = read(rd, buf, sizeof(buf));

        if (n > 0) { // si se leyeron datos, los procesa igual que el resto de los codigos, procesando en caso de que haya multiples lineas
            carry.append(buf, buf + n);

            std::size_t pos = 0;
            while (true) {
                std::size_t nl = carry.find('\n', pos);
                if (nl == std::string::npos) { break; }

                std::string line = carry.substr(pos, nl - pos);
                pos = nl + 1;

                try {
                    pid_t target = static_cast<pid_t>(std::stol(line));
                    int c = ++countByPid[target];

                    std::cout << "[MOD] Reporte a PID " << target << " (" << c << "/10)\n";

                    if (c >= 10) {
                        if (kill(target, SIGKILL) == 0) {
                            std::cout << "[MOD] PID " << target << " expulsado (SIGKILL)\n";
                        } else {
                            std::perror("[MOD] kill");
                        }
                        countByPid.erase(target);
                    }
                }
                catch (...) {
                    std::cerr << "[MOD] Línea inválida: " << line << "\n";
                }
            }

            carry.erase(0, pos);
            continue;
        }

        if (n == 0) { // si se leyó 0, todos los escritores cerraron, por lo que reabre la FIFO para seguir esperando
            close(rd);
            rd = open(kReportsPath, O_RDONLY);
            if (rd < 0) {
                std::perror("[MOD] reopen(reports RD)");
                break;
            }
            continue;
        }

        if (errno == EINTR) { continue; }
        std::perror("[MOD] read(reports)");
        break;
    }

    if (keepAlive >= 0) { close(keepAlive); }
    if (rd >= 0) { close(rd); }
    return 0;
}