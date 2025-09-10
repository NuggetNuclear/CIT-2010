<p align="center">
  <img src="https://img.shields.io/badge/status-en%20progreso-informational" alt="status">
  <img src="https://img.shields.io/badge/CIT2010-8A2BE2">
  <img src="https://img.shields.io/badge/roadmap-15%25-blue" alt="roadmap">
</p>

<h1 align="center">📦 Sistemas Operativos #1</h1>

<p align="center">
Tarea N°1 Sistemas Operativos: implementar un <b>chat comunitario en C++</b> con procesos independientes (clientes), un proceso central (orchestrator) y un proceso moderador encargado de sancionar procesos reportados.
</p>

---

## 🗺️ Roadmap

**Progreso general**  
<progress value="10" max="100"></progress> **10%**

---

### Fase 1 — Comunicación básica
- ✅ [T-001] **Orchestrator + cliente inicial con named pipes** — *10%*  
  Enlace: [commit 592bd7c](https://github.com/NuggetNuclear/CIT-2010_Tarea-1/tree/592bd7c00f18690340dc3c23eeca26f3b898190b)  
- ✅ [T-002] **Bidireccionalidad completa (lectura/escritura en ambos extremos)** — *5%*  
  Enlace: [commit B39172A](https://github.com/NuggetNuclear/CIT-2010_Tarea-1/tree/b39172aefa152e243a02de2dd3da2ee1f15d58b1)
### Fase 2 — Procesamiento de mensajes
- ⬜ [T-003] **Parsear y mostrar mensajes en el orchestrator** — *8%*  
- ⬜ [T-004] **Implementar validación y manejo de errores** — *7%*  

### Fase 3 — Broadcast (comunidad)
- ⬜ [T-005] **Redirigir mensajes de un cliente a todos los demás** — *10%*  
- ⬜ [T-006] **Formatear mensajes (nick, pid, hora)** — *5%*  

### Fase 4 — Persistencia en chat
- ⬜ [T-007] **Mantener cliente vivo tras enviar mensajes** — *8%*  
- ⬜ [T-008] **Multiplexación I/O (`select`/`poll`)** — *12%*  

### Fase 5 — Moderación
- ⬜ [T-009] **Proceso moderador independiente** — *5%*  
- ⬜ [T-010] **Clientes pueden enviar `reportar pid`** — *4%*  
- ⬜ [T-011] **Contador de reportes + expulsión a los 10** — *6%*  

### Fase 6 — Replicación de clientes
- ⬜ [T-012] **Funcion de compartir chat (Duplicar cliente)** — *13%*   

### Fase 7 — Pruebas finales
- ⬜ [T-013] **Pruebas de desconexión y cierre limpio de pipes** — *5%*  
- ⬜ [T-014] **Pruebas de estrés (múltiples clientes simultáneos)** — *2%*  

---