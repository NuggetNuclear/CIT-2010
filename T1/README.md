# Sistemas Operativos — Chat comunitario (T1)

**Plataforma:** Linux (Ubuntu) • **Lenguaje:** C/C++17 • **IPC:** FIFOs (named pipes) • **Multiplexación:** `select()` • **Sin threads**

---

Este repositorio implementa un **chat comunitario multi‑proceso** sin hilos, basado en **pipes con nombre (FIFOs)** en Linux/UNIX. Consta de tres binarios:

- `central`: orquestador que recibe mensajes de todos los clientes y los re‑difunde (broadcast).
- `client`: proceso independiente que se conecta al orquestador, envía/recibe mensajes y puede **autoclonarse**.
- `moderator`: proceso anexo que recibe reportes y **expulsa** (mata) a PIDs con ≥10 reportes.

> Cumple las restricciones de la tarea: sin threads, sin mutex ni semáforos; comunicación exclusivamente por **FIFOs**.

---

## Requisitos
- Linux/UNIX o WSL (Ubuntu recomendado).
- `g++` con soporte C++17.
- Permisos para crear FIFOs en `/tmp`.

Rutas usadas (fijas):
- **Clientes → Central**: `/tmp/orch_c2o.fifo`
- **Central → Cliente** (una por PID): `/tmp/orch_o2c_<PID>.fifo`
- **Central → Moderator**: `/tmp/orch_reports.fifo`

---

## Compilación
Ejecuta desde la carpeta del repo:

```bash
# Compilar cada componente
g++ central.cpp -o central
g++ moderator.cpp -o moderator
g++ client.cpp -o client
```

Si se esta usando WSL, compilar dentro de la distro Linux.

---

## Ejecución mínima
1) **Orquestador** (lanza también al moderador):
```bash
./central
```
> `central` intenta ejecutar `moderator` mediante `execlp("moderator", ...)`. Asegurarse de que el binario **esté en el mismo directorio**, de no ser asi, ejecutara sola la central, dejara un mensaje en la terminal, ejecutar el moderador con ./moderator.

2) **Clientes** (en **distintas terminales**):
```bash
./client
```

3) **Comandos del cliente**:
- `/leave` — salir del chat (termina el proceso actual).
- `/share` — **duplica** el cliente (fork + exec), creando otro proceso cliente.
- `/report <pid>` — reporta mala conducta del PID indicado. Con 10 reportes totales, `moderator` lo expulsa con `SIGKILL`.

4) **Limpieza** (opcional, si algo quedó en `/tmp/`):
```bash
rm -f /tmp/orch_c2o.fifo /tmp/orch_reports.fifo /tmp/orch_o2c_*.fifo
```

---

## Arquitectura y flujo de datos

### Vista general
```
[ client N ] --write--> /tmp/orch_c2o.fifo --read--> [ central ]
      |                                               |
      | <--read-- /tmp/orch_o2c_N.fifo <--write--     |
      '-----------------------------------------------'

[ central ] --write pid--> /tmp/orch_reports.fifo --read--> [ moderator ]
                                                (cuenta reportes y mata al PID≥10)
```

1. Cada **cliente** se identifica por su **PID** y crea su FIFO de bajada: `/tmp/orch_o2c_<PID>.fifo`.
2. Todos los clientes **escriben** al FIFO común `/tmp/orch_c2o.fifo` (uplink).
3. `central` **parsea** líneas del tipo `"[PID]-mensaje"`, registra al PID si es nuevo y **re‑difunde** el mensaje a **todas** las FIFOs de cliente (excepto al emisor). Además envía `ACK` al remitente.
4. Si el mensaje empieza con `"reportar "`, `central` reenvía **solo el PID objetivo** a `/tmp/orch_reports.fifo`.
5. `moderator` cuenta reportes por PID; al llegar a **10** envía `SIGKILL` al proceso reportado y limpia el contador.

### Protocolo de mensajes
- **Cliente → Central**: texto con formato `"[<pid>]-<contenido>\n"`.
- **Central → Cliente**: mensajes de broadcast `"[HH:MM:SS][PID <p> ] <contenido>\n"` y `ACK`/bienvenida.
- **Central → Moderator**: líneas con solo el `pid` objetivo terminado en `\n`.

### Robustez y señales
- `SIGPIPE` **ignorado** (evita abortar ante escritor/lector caído).
- `SIGINT` (Ctrl+C) provoca salida ordenada.
- Re‑aperturas de FIFOs si el otro extremo cierra (`read==0`).

---

## Algoritmo de planificación usado (importante)

**Enrutamiento en `central`**:
- **Multiplexación por eventos** con `select(2)` sobre el FIFO de entrada (C2O). El orquestador es un **bucle reactivo** que procesa mensajes **en orden de llegada**.
- **Política FCFS por mensaje** (First‑Come, First‑Served, no‑preemptiva): cada línea leída se parsea y se maneja completa antes de leer la siguiente. Los **writes** de los clientes son atómicos a nivel de línea (tamaño << `PIPE_BUF`), evitando intercalados.
- **Backpressure**: el descriptor hacia `moderator` se abre en **bloqueante**; si `moderator` va más lento, el `write()` se atasca y regula el flujo de reportes.
- **Broadcast**: entrega a clientes en una pasada (`unordered_map` por PID). El orden entre destinatarios no está definido (no afecta la semántica), pero **cada mensaje** se entrega a **todos**.

**En `client`**:
- `select(2)` **demultiplexa** entre **STDIN** y su FIFO de bajada (O2C). Esto evita hilos y **hambrunas** entre I/O humano y redifusión.

> Resumen: **Planificación por eventos + FCFS** a nivel de mensajes, con atomicidad de escritura en FIFO y presión inversa natural del sistema para moderar picos.

---

## Checklist de la rúbrica

1) **Proceso central gestiona el log/conversación**: `central` recibe, añade timestamps, escribe en STDOUT y **re‑difunde** los mensajes (`broadcastMessage`).

2) **Número indeterminado de procesos independientes**: Cada `./client` es un proceso autónomo; pueden lanzarse arbitrariamente en terminales distintas. `central` no es padre de los clientes.

3) **Comunicación por pipes bidireccionales**: Uplink común `/tmp/orch_c2o.fifo` y downlink por‑PID `/tmp/orch_o2c_<pid>.fifo`.

4) **Salida libre de los procesos**: El usuario teclea `/leave` o `Ctrl+D`; también `SIGINT` termina ordenado.

5) **Autocopia de procesos**: Comando `/share` en `client` hace `fork()+exec()` del mismo binario, que se conecta como otro participante.

6) **Proceso anexo de reportes**: `moderator` lee `/tmp/orch_reports.fifo`, cuenta por PID y mata con `SIGKILL` al llegar a 10.

7) **README**: Explica ejecución, funcionamiento y **algoritmo de planificación**.

> Cada punto está implementado con FIFOs y sin uso de hilos/mutex/semaforización.

---

## Explicación de código por archivo

### `central.cpp`
**Tipos y constantes**
- `using FdByPid = std::unordered_map<pid_t, int>;` — Mapa O(1) de **PID → FD** de escritura hacia cada cliente. Se prefiere a un `std::map` (logN) o a vectores esparcidos, por eficiencia con PIDs no contiguos.
- Rutas constantes a FIFOs (`kC2oPath`, `kReportsPath`). `constexpr const char*` evita copias y deja claro que son inmutables.
- `volatile sig_atomic_t stopRequested` — Tipo seguro para señales; evita estados intermedios al modificar flags desde handler.

**Funciones clave**
- `ensureFifoExists(path)` — Crea FIFO si no existe. tolera `EEXIST`.
- `getO2cPathFor(pid)` — Forma la ruta por cliente sin asignaciones globales.
- `openWriteToClient(pid)` — Garantiza la FIFO del cliente y abre **O_WRONLY|O_NONBLOCK**, cambiando luego a **bloqueante** si aplica para backpressure de entrega.
- `nowHms()` — Timestamp local `HH:MM:SS`
- `parseMessage(raw, pid, msg)` — Parser de `"[pid]-mensaje"` que valida dígitos y separadores. 
- `sendAck(map, pid)` — Escribe `ACK` a emisor; si falla el `write()`, cierra y **retira** el FD del mapa.
- `broadcastMessage(map, sender, msg)` — Escribe a todos menos al emisor, iterando el `unordered_map`. Si un `write()` falla, cierra y borra la entrada; evita acumular FDs colgados.
- `spawnModerator()` — `fork()` + `execlp("moderator")`
- `openReportsWriter()` — Abre `/tmp/orch_reports.fifo` en **no bloqueante** con reintentos hasta que exista un lector; luego remueve `O_NONBLOCK` para tener flujo regulado.
- `handleReportIfAny(message, reportsFd)` — Reconoce prefijo `reportar `, extrae PID target y escribe la línea al FIFO del `moderator`. Devuelve `true` si consumió el mensaje (no se hace broadcast de reportes).
- `processLine(raw, map, reportsFd)` — Punto central: alta de cliente (abre O2C si es nuevo), logging en `stdout`, manejo de `/report`, broadcast y `ACK`.
- `processChunk(buf, n, ...)` — Ensambla líneas completas (maneja múltiples mensajes en un solo `read`). Evita truncamientos por límites de buffer.
- `main()` — Configura señales, garantiza FIFOs, lanza `moderator`, abre C2O en `O_RDONLY|O_NONBLOCK` + un WR **keep‑alive** (evita EOF cuando no hay escritores), y entra en bucle `select()`/`read()`; al final, cierra todo y **mata** al `moderator` con `SIGTERM`.

---

### `moderator.cpp`
**Tipos y constantes**
- `std::unordered_map<pid_t,int> countByPid` — Contador O(1) por PID de reportes.
- `volatile sig_atomic_t stopRequested` + handler `SIGINT` — Salida ordenada y segura.

**Flujo**
1. Garantiza `/tmp/orch_reports.fifo`.
2. Abre lectura **bloqueante** y un **WR keep‑alive** (no esencial, pero estabiliza el comportamiento si se cierran todos los escritores).
3. Lee en bucle, acumulando en `carry` y procesando **línea a línea**.
4. Incrementa contador por PID; al llegar a **10**, envía `kill(pid, SIGKILL)` y borra el registro.

**Por qué así**
- `SIGKILL` garantiza expulsión inmediata (no depende de cooperación del proceso objetivo).
- Parseo manual de líneas y manejo de `read==0` para **re‑abrir** la FIFO: tolera que `central` se reinicie.

---

### `client.cpp`
**Tipos y helpers**
- `volatile sig_atomic_t stopRequested` — Permite interrupción con `Ctrl+C`.
- Helpers de texto: `startsWith`, `trim` para comandos.

**Apertura de canales**
- `openMyDownlink(o2cPath)` — Crea/abre la FIFO **propia** en `O_RDWR`. _Por qué `O_RDWR`_: abrir una FIFO solo para lectura puede **bloquear** si todavía no hay escritor; `O_RDWR` evita ese bloqueo y mantiene la FIFO viva mientras llega `central`.
- `openUplinkWriter(c2o)` — Abre `/tmp/orch_c2o.fifo` en **no bloqueante** con reintentos hasta que `central` esté listo; luego quita `O_NONBLOCK` para que los `write()` bloqueen si se llena (backpressure en uplink).

**Protocolo de envío**
- `sendConnectHello/DisconnectBye` — Mensajes de control al iniciar/salir.
- `sendUserMessage(fd, pid, text)` — Empaqueta como `"[pid]-texto\n"`; maneja `EPIPE` si `central` no está disponible.

**Interfaz y comandos**
- `handleIncomingFromCentral(o2cFd)` — Lee, imprime y **redibuja** el prompt `> `.
- `handleUserInput(c2oFd, pid, programPath)` —
  - `/leave` — cierra ordenado.
  - `/share` — `fork()` y `execlp(argv[0])` para **replicar** el cliente.
  - `/report <pid>` — valida y convierte a `"reportar <pid>"` (interpretado por `central`).
  - Otro texto — se envía tal cual.
- `runChatLoop(...)` — Bucle con `select()` entre **STDIN** y **O2C**.



## Troubleshooting
- **`central: execlp(moderator)` falló** → ejecuta `./moderator` manualmente en otra terminal o añade el directorio al `PATH`.
- **Permisos en `/tmp`** → ejecuta los binarios con el usuario que tenga permisos para crear FIFOs.
- **FIFOs huérfanas** → elimina las rutas listadas arriba y vuelve a ejecutar.

---