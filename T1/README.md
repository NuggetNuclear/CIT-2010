# Sistemas Operativos — Chat comunitario (T1)

**Plataforma:** Linux (Ubuntu) • **Lenguaje:** C/C++17 • **IPC:** FIFOs (named pipes) • **Multiplexación:** `select()` • **Sin threads**

---

## TL;DR — Checklist de la rúbrica (cumplimiento rápido)

- **Procesos requeridos:** `central` (orquestador), `client` (N instancias), `moderator` (conteo de reportes) — *implementados*.
- **Comunicación por FIFOs:** C2O común (`/tmp/orch_c2o.fifo`), O2C por PID (`/tmp/orch_o2c_<PID>.fifo`), y `reports` (`/tmp/orch_reports.fifo`) — *implementado*.
- **Protocolo línea‑a‑línea:** `"[PID]-<texto>\n"`; ACK al emisor y broadcast a otros — *implementado*.
- **Clientes independientes en terminales distintas:** conexión/desconexión libre (`/leave` o EOF) — *implementado*.
- **Moderación/expulsión:** `/report <pid>` suma en `moderator`; a la 10.ª denuncia → `SIGKILL` — *implementado*.
- **Planificación/explicación técnica:** `select()` (bloqueo eficiente), difusión **FCFS** por orden de llegada en C2O — *documentado aquí*.
- **Ejecución reproducible:** instrucciones con **Makefile** y **g++** directo — *incluidas*.
- **Restricciones del enunciado:** UNIX, FIFOs, sin hilos/semáforos; manejo de señales/errores — *cumplidas*.

---

## 1) Cumplimiento del enunciado

1. **Múltiples procesos independientes:** `central`, `client` (N terminales), `moderator`.
2. **IPC por pipes con nombre (FIFOs):**
   - **C2O** `/tmp/orch_c2o.fifo` (clientes → central, uplink común).
   - **O2C** `/tmp/orch_o2c_<PID>.fifo` (central → cliente, downlink por PID).
   - **reports** `/tmp/orch_reports.fifo` (central → moderator).
3. **Formato de mensajes y difusión:** cada línea del cliente llega al `central`, este **ACK** al emisor y **difunde** a los demás.
4. **Comandos mínimos:**
   - `/leave`: termina el cliente limpiamente.
   - `/report <pid>`: informa al moderador (no se difunde).
   - `/share`: abre **otro** cliente (nuevo proceso) con `fork()+execlp()`.
5. **Moderación:** `moderator` acumula reportes por PID; al llegar a **10**, intenta `kill(SIGKILL)` y reinicia el contador.
6. **Planificación / concurrencia:** sin threads; `select()` para multiplexar I/O; orden de servicio **FCFS** según llegada a C2O.
7. **Robustez:** ignorar `SIGPIPE`, reaperturas tras `EOF`, keep‑alive en FIFOs, reintentos `ENXIO/ENOENT`, framing por líneas.
8. **README:** contiene explicación de ejecución, funcionamiento e **hipótesis/decisiones de diseño** (esta sección).

---

## 2) Arquitectura general

**Procesos**
- `central`: orquesta, registra y difunde. Mantiene `PID → FD(O2C)`. Envía **ACK** al emisor y **broadcast** al resto. Abre `reports` y lanza `moderator` al inicio.
- `client`: instancia interactiva (N terminales). Escribe en C2O, lee su O2C, ejecuta comandos (`/leave`, `/share`, `/report`).
- `moderator`: lee PIDs desde `reports`, cuenta por PID y expulsa con `SIGKILL` al 10.º reporte.

**FIFOs**
- **C2O** `/tmp/orch_c2o.fifo`
- **O2C** `/tmp/orch_o2c_<PID>.fifo`
- **reports** `/tmp/orch_reports.fifo`

**Protocolo**
- Cliente → Central: `"[<pid>]-<texto>\n"`
- Respuestas del central:
  - **ACK** al emisor: `"[CENTRAL HH:MM:SS] ACK\n"`
  - **Broadcast** a otros: `"[HH:MM:SS][PID <pid>] <texto>\n"`

---

## 3) Compilación y ejecución

### 3.2 Compilación **manual**
```bash
g++ central.cpp -o central
g++ client.cpp -o client
g++ moderator.cpp -o moderator
```

Ejecución (en terminales separadas, mismo flujo que arriba):
```bash
./central
./client
./client   # opcional: más clientes
./moderator   # si central no lo lanza automáticamente
```

**Limpieza de FIFOs (si quedaran colgadas):**
```bash
rm -f /tmp/orch_c2o.fifo /tmp/orch_o2c_*.fifo /tmp/orch_reports.fifo
```

---

## 4) Planificación y manejo de concurrencia

- **Central**: espera datos en **C2O** con `select()` (bloqueo eficiente). Al recibir, procesa y difunde en **orden de llegada (FCFS)**.
- **Cliente**: `select()` sobre **STDIN** y **su O2C** para leer difusiones mientras el usuario escribe.
- **Sin threads**: sólo procesos + multiplexación. Evita condiciones de carrera a nivel de memoria compartida.

---

## 5) Robustez (señales, errores, aperturas)

- **`SIGPIPE` ignorada**: evita abortos en `write()` sin lector; se maneja **`EPIPE`** cerrando y limpiando.
- **Reintentos** al abrir en `O_WRONLY|O_NONBLOCK` (casos `ENXIO/ENOENT`) hasta que el extremo remoto exista; luego **limpiar `O_NONBLOCK`** con `fcntl` para volver a escrituras bloqueantes.
- **Keep‑alive** en FIFOs: abrir el otro extremo en no‑bloqueante para evitar `read==0` constante.
- **Reapertura tras `EOF`** en el lado lector para seguir atendiendo nuevos escritores.
- **Framing por líneas** con buffer de arrastre: procesar **solo** líneas completas terminadas en `\n`.

---

## 6) Funciones principales (por archivo)

### 6.1 `client.cpp`
- `openMyDownlink(o2cPath)`: abre O2C en `O_RDWR` (evita bloqueo de arranque).
- `openUplinkWriter(c2oPath)`: abre C2O en `O_WRONLY|O_NONBLOCK` con reintentos y luego limpia `O_NONBLOCK`.
- `sendConnectHello / sendDisconnectBye / sendUserMessage`: serializan `"[PID]-..."` y envían por C2O; manejan `EPIPE`.
- `startsWith / trim`: utilidades de comandos.
- `handleIncomingFromCentral(o2cFd)`: imprime ACKs/difusiones desde O2C sin romper el prompt.
- `handleUserInput(c2oFd, pid, programPath)`: comandos `/leave`, `/share` (fork+exec), `/report <pid>`.
- `runChatLoop(...)`: bucle con `select()` (STDIN + O2C) y bandera de salida por `SIGINT`.

### 6.2 `central.cpp`
- `ensureFifoExists(path)`, `getO2cPathFor(pid)`, `openWriteToClient(pid)`.
- `nowHms()`: hora local `HH:MM:SS` segura (`localtime_r`).
- `parseMessage(raw, pidOut, msgOut)`: extrae `pid` y cuerpo de `"[pid]-..."`.
- `sendAck(...)`: ACK al emisor; si falla, cierra y limpia el FD del mapa.
- `broadcastMessage(...)`: difunde a **todos menos el emisor**; limpia FDs que fallen.
- `openReportsWriter()`: apertura robusta de `reports` (reintentos + limpiar `O_NONBLOCK`).
- `handleReportIfAny(message, reportsFd)`: si empieza por `"reportar "`, envía `<pid>\n` a `reports` y **no** difunde.
- `processLine(...) / processChunk(...)`: parseo → bienvenida (nuevo PID) → log → (reporte|broadcast) → ACK.
- `spawnModerator()`: `fork()+execlp("moderator", ...)`.
- `main()`: señales, FIFOs, bucle `select()` sobre C2O, manejo de `EINTR/EAGAIN`, reaperturas y limpieza final.

### 6.3 `moderator.cpp`
- `ensureFifoExists(kReportsPath)`: garantiza FIFO de reportes.
- Apertura `O_RDONLY` + **keep‑alive** `O_WRONLY|O_NONBLOCK` sobre `reports`.
- Bucle de lectura con **buffer** y `carry` para líneas completas terminadas en `\n`.
- `std::unordered_map<pid_t,int>`: contador por PID; al llegar a **10**, `kill(pid, SIGKILL)` y borrar del mapa.
- Manejo de errores: líneas inválidas, `read==0` → reabrir, `EINTR` → continuar, otros → `perror` y salir.

---

## 7) Variables y estructuras clave

### 7.1 `central.cpp`
- `FdByPid writerByPid` — `unordered_map<pid_t,int>`: **PID → FD(O2C)**; se limpia en fallos de `write()`.
- `const char* kC2oPath`, `kReportsPath` — rutas de FIFOs comunes.
- `int reportsFd` — descriptor de `reports`.
- `int c2oRd`, `int keepAlive` — extremos de lectura y keep‑alive para C2O.
- `char buf[4096]` — buffer de lectura desde C2O; procesado por `processChunk`.
- `volatile sig_atomic_t stopRequested` — bandera de salida por `SIGINT`.

### 7.2 `client.cpp`
- `int o2cFd` — lector de O2C (downlink).
- `int c2oFd` — escritor C2O (uplink).
- `pid_t myPid` — PID local para framing.
- Helpers: `startsWith`, `trim`.

### 7.3 `moderator.cpp`
- `int rd` — lector de `reports`.
- `int keepAlive` — escritor no‑bloqueante auxiliar (keep‑alive de FIFO).
- `std::unordered_map<pid_t,int> countByPid` — contadores por PID.
- `std::string carry` — buffer de arrastre para recomponer líneas.

---

## 8) Entorno

- Probado en **Ubuntu 24.04**.
- Sin hilos ni semáforos; solo procesos + FIFOs + `select()`.
