
# 📦 Sistemas Operativos — Chat comunitario
**Tres procesos:** `central` (orquestador), `client` (N terminales), `moderator` (contador de reportes). Comunicación por **FIFOs (named pipes)**; multiplexación de I/O con **`select()`**; manejo de señales (`SIGINT`, `SIGPIPE`) y errores (`EINTR`, `EPIPE`, `EAGAIN/EWOULDBLOCK`).

> Desarrollado en Linux 🐧 (Ubuntu 24.04).

---

## 0) Las pipes 


- **C2O** (`/tmp/orch_c2o.fifo`): *uplink* común (clientes → central).
- **O2C** (`/tmp/orch_o2c_<PID>.fifo`): *downlink* por cliente (central → ese cliente).
- **reports** (`/tmp/orch_reports.fifo`): central → moderator (líneas con PIDs reportados).

---

## 1) Compilación y ejecución

```bash
g++ central.cpp -o central
g++ client.cpp -o client
g++ moderator.cpp -o moderator
```

Lanzar en terminales separadas, en este orden:

```bash
./central
./client   # abre un cliente (se puede abrir mas de uno)
# opcional, ya que central debe lanzar el proceso automaticamente:
./moderator
```

Limpieza de FIFOs:
```bash
rm -f /tmp/orch_c2o.fifo /tmp/orch_o2c_*.fifo /tmp/orch_reports.fifo
```

---

## 2) Protocolo de línea (cliente → central)

Cada envío es **una línea** terminada en `\n` con el formato:
```
[<pid>]-<texto>\n
```

Tipos de mensajes del cliente:
- Conexion: `"[PID]-Proceso conectado\n"` (enviado automáticamente al conectar).
- Mensaje normal: `"[PID]-<texto>\n"`.
- Reporte: comando `/report <pid>` ⇒ `"[PID]-reportar <pid>\n"`.
- Desconexión: `/leave` ⇒ `"[PID]-Proceso desconectado\n"`.

Respuestas de la central:
- **ACK al emisor**: `"[CENTRAL HH:MM:SS] ACK\n"`
- **Broadcast a otros**: `"[HH:MM:SS][PID <pid>] <texto>\n"`

---

## 3) Flujo detallado de mensajes

**Algoritmo de planificación**
La central procesa eventos en orden de llegada (FIFO) sobre el canal C2O. La multiplexación es con select() (E/S no bloqueante). Cada mensaje se delimita por \n, se reconoce el emisor por PID y se difunde de forma serializada a todos los demás clientes. No hay prioridades.

### 3.1 Conexión de cliente
1. `client` crea/abre su **O2C** (`/tmp/orch_o2c_<PID>.fifo`) en **RDWR** para evitar bloqueos si el otro extremo aún no está listo (8.1).
2. Abre **C2O** en **WR** con reintentos; limpia `O_NONBLOCK` para simplificar escrituras (8.2).
3. Envía línea “Proceso conectado” por C2O.
4. `central` detecta el PID; si no tiene FD para ese PID, **abre O2C** y guarda `writerByPid[PID] = FD`.
5. `central` envía **welcome** al O2C del nuevo cliente; el cliente lo muestra y deja un `prompt`.

### 3.2 Mensajes normales
1. `client` envía `"[PID]-texto\n"`.
2. `central` parsea → **ACK** al emisor y **broadcast** a los demás clientes.
3. Por fin se imprime bien y no rompe la terminal😭.

### 3.3 Reportes (`/report <pid>`)
1. `client` valida y envía: `"[PID]-reportar <pid>\n"`.
2. `central` no difunde; **escribe `<pid>\n`** al **reports** FIFO.
3. `moderator` acumula contadores por PID; cuando alcanza **10** reportes, ejecuta `kill(pid, SIGKILL)` y reinicia el contador.

### 3.4 Desconexión
- Con `/leave` o EOF en `stdin`, el cliente envía “Proceso desconectado”, cierra descriptores y termina.
- `central` borrará el FD del mapa cuando la próxima escritura a ese cliente falle.

---

## 4) Diseño de robustez (bloqueos, keep‑alive, reintentos)

- **Ignorar `SIGPIPE`** en los tres codigos: evita abortar si se escribe sin lector, se manejan errores `EPIPE` en `write()` (10.4).  
- **Keep‑alive de FIFO**: se abre el otro extremo en **no bloqueante** para evitar **EOF** cuando no hay escritores/lectores; p.ej. `moderator` abre `reports` en `O_WRONLY|O_NONBLOCK` además del `O_RDONLY`, solo un ejemplo.
- **Reapertura tras EOF** (`read(...) == 0`): reabrir lado lector (`open(O_RDONLY)`) mantiene el servicio esperando nuevos escritores (10.2).
- **Reintentos con `ENXIO`/`ENOENT`** al abrir en `O_WRONLY|O_NONBLOCK`: el servidor/lector puede no estar listo todavía (8.2).

---

## 5) Funciones

### 5.1 `client.cpp`

- `static volatile sig_atomic_t stopRequested;  static void handleSigint(int)`  
  Bandera de salida segura ante `SIGINT` (Ctrl+C). Tipo `sig_atomic_t` garantiza lecturas/escrituras atómicas en presencia de señales (9.2).

- `getC2oPath() / getO2cPathFor(pid)`  
  Utilidades de **rutas** de FIFO: C2O común y O2C por PID.

- `ensureFifoExists(path, mode=0666)`  
  Crea FIFO con `mkfifo` si no existe (acepta `EEXIST`).

- `openMyDownlink(o2cPath)`  
  Abre **O2C** del cliente en **`O_RDWR`**. Abrir en lectura *y* escritura evita bloqueos si el escritor (central) aún no está conectado (8.1).

- `openUplinkWriter(c2oPath)`  
  Abre **C2O** en `O_WRONLY|O_NONBLOCK` con **reintentos** si `ENXIO/ENOENT`; luego **limpia `O_NONBLOCK`** usando `fcntl(F_SETFL, flags & ~O_NONBLOCK)` para tener escrituras bloqueantes “normales” (8.2).

- `sendConnectHello / sendDisconnectBye / sendUserMessage`  
  Serializan la línea `"[PID]-..."` y la envían por C2O. Manejan `EPIPE` con mensaje default.

- `startsWith / trim`  
  necesario para comandos (`/share`, `/report`, etc.).

- `handleIncomingFromCentral(o2cFd)`  
  Lee difusiones/ACKs desde O2C y las imprime sin arruinar el prompt de la terminal.

- `handleUserInput(c2oFd, pid, programPath)`  
  Procesa:  
  - `/leave` ⇒ bye + salir.  
  - `/share` ⇒ `fork()` + `execlp()` del mismo codigo (nuevo cliente con otro PID).  
  - `/report <pid>` ⇒ “reportar pid”.  
  - Cualquier otra cosa ⇒ mensaje normal.

- `runChatLoop(c2oFd, o2cFd, pid, programPath)`  
  Bucle principal con **`select()`** sobre `STDIN` + O2C (8.3). reacciona a `stopRequested`.

### 5.2 `central.cpp`

- `static volatile sig_atomic_t stopRequested;  static void handleSigint(int)`  
  Salida ordenada ante `SIGINT` (9.2).

- `kC2oPath`, `kReportsPath`  
  Rutas **constexpr** para asegurar inmutabilidad y uso en tiempo de compilación.

- `ensureFifoExists(path)`  
  Crea FIFOs necesarias (`/tmp/orch_c2o.fifo` y `/tmp/orch_reports.fifo`).

- `getO2cPathFor(pid)`  
  Ruta de O2C específica de cada cliente.

- `openWriteToClient(pid)`  
  Asegura la O2C y la abre `O_WRONLY|O_NONBLOCK` (eliminar bloqueos de arranque).

- `nowHms()`  
  Marca temporal `HH:MM:SS` segura con `localtime_r`.

- `parseMessage(raw, pidOut, msgOut)`  
  Valida el formato `"[pid]-..."` y extrae PID + cuerpo. Maneja excepciones de `stoi` (string to int) y errores de formato.

- `sendAck(writerByPid, senderPid)`  
  Escribe `ACK` al O2C del emisor; si falla la escritura, cierra y elimina el FD del mapa `writerByPid`.

- `broadcastMessage(writerByPid, senderPid, message)`  
  Difunde a **todos menos el emisor**. Si `write` falla, cierra y borra el FD de ese receptor.

- `spawnModerator()`  
  `fork()` + `execlp("moderator", ...)`. Si falla, avisa pero no aborta el central (probado cambiando el nombre del mod).

- `openReportsWriter()`  
  Abre `reports` con reintentos (`ENXIO/ENOENT`), limpia `O_NONBLOCK`. Devuelve FD listo para `write` bloqueante.

- `handleReportIfAny(message, reportsFd)`  
  Si `message` comienza con `"reportar "`, extrae PID y escribe `"<pid>\n"` en **reports**. Devuelve `true` si consumio el mensaje (se envía solo ACK).

- `processLine(rawLine, writerByPid, reportsFd)`  
  Orquesta todo: parseo → bienvenida si es nuevo PID → log → si es reporte, **reportar + ACK**; si no, **broadcast + ACK**.

- `processChunk(buffer, bytesRead, writerByPid, reportsFd)`  
  Parte un bloque en **lineas terminadas en `\n`** y llama `processLine` por cada una (soporta múltiples líneas por lectura).

- `main()`  
  Configura señales, garantiza FIFOs, lanza moderador, abre **C2O** en `O_RDONLY|O_NONBLOCK` + **keep‑alive** `O_WRONLY|O_NONBLOCK`, y entra a un bucle `select()` sobre **C2O**. Maneja `EINTR`, `EAGAIN/EWOULDBLOCK`, y reabre C2O tras EOF.

### 5.3 `moderator.cpp`

- `ensureFifoExists(kReportsPath)`  
  Garantiza FIFO de reportes.

- `open(kReportsPath, O_RDONLY)` + **`keepAlive = open(..., O_WRONLY|O_NONBLOCK)`**  
  Patrón de **keep‑alive**: si no hay escritores reales, evitar `n == 0` permanente al leer.

- Bucle de lectura con **buffer** + `carry`  
  Acumula bytes hasta encontrar `\n`, extrae **líneas completas** (PID en texto).

- Contador `std::unordered_map<pid_t, int>`  
  `++countByPid[target]` por línea válida. Si llega a **10**, intenta `kill(target, SIGKILL)` y **borra** la entrada del mapa.

- Errores:  
  - Línea inválida ⇒ registro y continuar.  
  - `read == 0` ⇒ reabrir RD.  
  - `EINTR` ⇒ continuar.  
  - Otros ⇒ `perror` y salir.

---

## 6) tipos, flags y llamadas usadas (varias son nuevas para mi, pero se entienden con la documentacion)

### 6.1 Tipos POSIX y C++
- **`pid_t`** (`<sys/types.h>`): identificador de proceso. Aquí etiqueta a cada cliente, y también a los PIDs reportados.
- **`ssize_t`** (`read`/`write` devuelven `ssize_t`): permite distinguir **error** (`-1`), **EOF** (`0`) y **bytes positivos** leídos/escritos. Es *signed* a diferencia de `size_t`; por eso sirve para reportar `-1` en errores.
- **`std::sig_atomic_t`** (`<csignal>`): entero atómico 💣 a nivel de señal; se usa **`volatile std::sig_atomic_t stopRequested`** para comunicación segura *handler ↔ bucle principal* ante `SIGINT`, supuestamente es la forma segura.
- **`std::unordered_map<pid_t,int>`**: contador por PID (O(1) promedio).

### 6.2 Flags y errores típicos
- **`O_NONBLOCK`** (`open`, `fcntl`)  
  - Para **abrir** una FIFO sin bloquear si el otro extremo no existe aún.  
  - Luego, en FDs de escritura, se “limpia” para volver a modo bloqueante (evita lidiar con EAGAIN en cada `write`).

- **`O_RDWR`** (cliente abre su O2C): evita bloqueo si no hay escritor todavía.

- **Errores**  
  - `EINTR`: llamada interrumpida por señal ⇒ reintentar.  
  - `EPIPE`: escritura en FIFO sin lector ⇒ manejar y cerrar (gracias a ignorar `SIGPIPE`, no aborta el proceso).  
  - `EAGAIN`/`EWOULDBLOCK`: no hay datos listos en no‑bloqueante ⇒ continuar o reintentar.  
  - `ENXIO`/`ENOENT` al abrir: extremo remoto aún no existe ⇒ **reintentos** con `usleep`.

### 6.3 Señales
- **`SIGPIPE` ignorada**: sin esto, un `write` a FIFO sin lector mata el proceso. Se opta por manejar **`EPIPE`** en el flujo normal.
- **`SIGINT` (Ctrl+C)**: handler minimalista que solo **marca una bandera**; el bucle principal la observa y sale ordenadamente.
- **`SIGKILL`**: no bloqueable ni capturable; el **moderator** lo envía a PIDs con ≥10 reportes.

---

## 7) Multiplexación con `select()`

- **Central:** espera datos en **C2O** (puede ampliarse a más FDs). Con `select()` se **bloquea** hasta que C2O esté listo para leer. Devuelve cuántos FDs están listos; si `EINTR`, se repite.
- **Cliente:** vigila **STDIN** y **O2C** simultáneamente. Así puede recibir mensajes **mientras** el usuario escribe. Se calcula `maxFd` y se pasa `maxFd+1` a `select()`.

---

## 8) Patrones de IPC usados (y por qué)

### 8.1 Abrir la FIFO del cliente (`O2C`) en **RDWR**
Abrir en **solo lectura** (`O_RDONLY`) bloquearía hasta que el central abra la escritura. Usando **`O_RDWR`** el cliente **nunca** se bloquea al arrancar, y más tarde el central abre `O_WRONLY` cuando el cliente se presenta. Este patrón estabiliza arranque/desconexión, de otra forma se hacen deadlocks, me pasó 💀💀.

### 8.2 Abrir la FIFO común (`C2O`) en **`O_WRONLY|O_NONBLOCK`** y luego **limpiar `O_NONBLOCK`**
Abrir en `O_WRONLY` puro bloquearía hasta que el central abra lectura. Con `O_NONBLOCK`, el `open` devuelve **inmediatamente**; si falla con `ENXIO/ENOENT`, se **reintenta** (el central aún no está listo). Una vez abierto, **limpiamos `O_NONBLOCK`** con `fcntl(F_SETFL, ...)` para que los `write()` sean bloqueantes simples y no lidiar con `EAGAIN` en el camino.

### 8.3 Protocolo **línea‑a‑línea** con buffer de arrastre
Los `read()` pueden entregar **múltiples** líneas o **fracciones** de una. Usamos:
- **`carry`** (moderator) y **`processChunk`** (central) para juntar bytes hasta `\n`.  
- Procesamos **solo líneas completas** y dejamos en buffer el resto.  
Esto evita “duplicar” o “romper” mensajes. Es streaming correcto.

---

## 9) Criterios

- Sin **threads**: simplifica condiciones de carrera; toda concurrencia es a nivel de procesos.
- Limpieza: cierre de FDs, borrado del mapa al expulsar para no crecer en memoria.

---

## 10) Errores y casos criticos

1. **EOF en FIFO (`read==0`)**: significa que no hay escritor/lector del otro lado. Reabrimos el extremo de lectura y seguimos esperando.
2. **`EINTR`**: `select`/`read` interrumpidos por señal (p.ej., `SIGINT`) ⇒ continuar; el bucle saldrá por la bandera.
3. **`EPIPE`** (escritura sin lector): se informa y se cierra el FD. Gracias a ignorar `SIGPIPE`, el proceso no aborta.
4. **`EAGAIN/EWOULDBLOCK`**: en no bloqueante, no hay datos/listo ⇒ seguir (cliente) o reintentar (aperturas).
5. **Líneas inválidas**: se loguean y descartan; el proceso **no** termina por entradas corruptas.
6. **`kill` fallido** (`ESRCH`): el proceso reportado ya no existe; se informa y se borra el contador igualmente.

---

## 11) Variables y estructuras 

### 11.1 `central.cpp`
- `FdByPid writerByPid` — `unordered_map<pid_t,int>`: mapea **PID → FD** de su **O2C**. Se depura al fallar escrituras.
- `const char* kC2oPath`, `kReportsPath` — rutas de FIFOs comunes.
- `char buf[4096]` — buffer de lectura desde C2O; procesado por `processChunk`.
- `int reportsFd` — descriptor de **reports** para hablar con `moderator`.
- `int c2oRd`, `int keepAlive` — extremos de lectura y *keep‑alive* de C2O.
- `volatile sig_atomic_t stopRequested` — bandera de salida por señal.

### 11.2 `client.cpp`
- `int o2cFd` — lector de mensajes desde el central (downlink del cliente).
- `int c2oFd` — escritor hacia el central (uplink común).
- `pid_t myPid` — PID local, usado en el framing de cada línea.
- Helpers de texto: `startsWith`, `trim`.

### 11.3 `moderator.cpp`
- `int rd` — lector de **reports**.
- `int keepAlive` — escritor no bloqueante para mantener viva la FIFO.
- `std::unordered_map<pid_t,int> countByPid` — contadores por PID.
- `std::string carry` — buffer de arrastre para recomponer líneas por `\n`.
