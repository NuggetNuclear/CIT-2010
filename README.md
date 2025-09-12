<p align="center">
  <img src="https://img.shields.io/badge/status-en%20progreso-informational" alt="status">
  <img src="https://img.shields.io/badge/CIT2010-8A2BE2">
</p>

<h1 align="center">📦 Sistemas Operativos #1 — Chat comunitario en C++</h1>

**Descripción breve**  
Chat de múltiples procesos en C++: un proceso central (orchestrator) que gestiona la conversación y múltiples procesos cliente, todos independientes y no emparentados, conectados mediante *named pipes* (FIFOs) **bidireccionales**. Sin hilos (threads), sin mutex ni semáforos; uso de `select()` para multiplexar I/O. Requiere entorno **UNIX**.  
*(Condiciones y requisitos según enunciado y rúbrica).*  

---

## 🗺️ Roadmap

### Fase 1 — Comunicación básica
- [x] Orchestrator + cliente inicial con named pipes
- [x] Bidireccionalidad (lectura/escritura en ambos extremos)

### Fase 2 — Procesamiento de mensajes
- [x] Parsear y mostrar mensajes en el orchestrator
- [x] Validación y manejo de errores básicos

### Fase 3 — Broadcast (comunidad)
- [x] Redirigir mensajes de un cliente a todos los demás
- [ ] Formatear mensajes (nick, pid, hora) // TODO

### Fase 4 — Persistencia en chat
- [x] Mantener cliente vivo tras enviar mensajes (loop interactivo)
- [x] Multiplexación I/O con `select`

### Fase 5 — Moderación
- [ ] Proceso moderador independiente // TODO
- [ ] Comando `reportar <pid>` y expulsión a los 10 reportes // TODO

### Fase 6 — Replicación de clientes
- [ ] Duplicar cliente (`/share`) // TODO

### Fase 7 — Pruebas finales
- [ ] Pruebas de desconexión y cierre limpio de pipes (más casos) // TODO
- [ ] Pruebas de estrés (múltiples clientes simultáneos) // TODO

---

## 🏗️ Compilación

```bash
g++ -std=c++17 central.cpp -o central
g++ -std=c++17 cliente.cpp -o cliente
