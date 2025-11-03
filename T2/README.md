# 🧩 Tarea 2 - Simulador de Héroes y Monstruos

**Autores:** Matías Vigneau, Gabriel González  
**Fecha:** 2 de noviembre de 2025

---

## 📘 Descripción general
Este proyecto implementa un **simulador concurrente** en C, donde héroes y monstruos interactúan dentro de un mapa definido por un archivo de configuración (`config.txt`).  
Cada héroe y monstruo se ejecuta en un hilo separado y se sincronizan mediante un **mutex compartido** para evitar conflictos durante la simulación.

El objetivo es representar cómo las entidades se mueven, detectan enemigos, atacan y terminan la simulación cuando todos los héroes mueren o escapan.

---

## 🗂️ Estructura del proyecto

```
Tarea2_SO/
├── src/
│   ├── main.c       # Lógica principal del simulador
│   ├── config.c     # Lectura y manejo del archivo de configuración
│   └── config.h     # Estructuras y prototipos de funciones
├── Makefile         # Compilación y ejecución automática
├── config.txt       # Archivo de configuración (editable para pruebas)
└── README.md        # Este documento
```

---

## ⚙️ Compilación y ejecución

1. **Compilar el programa**
   ```bash
   make
   ```

2. **Ejecutar el simulador**
   ```bash
   make run
   ```
   Esto ejecutará el programa usando el archivo `config.txt` del directorio principal.

3. **Probar distintos escenarios**
   - Para usar un ejemplo distinto, basta con **reemplazar el contenido de `config.txt`** por uno de los ejemplos de configuración (por ejemplo, `Ejemplo 1`, `Ejemplo 2`, etc.).
   - No es necesario cambiar el nombre del archivo ni modificar el código fuente.
   - Luego, simplemente vuelve a ejecutar:
     ```bash
     make run
     ```

4. **Limpiar archivos generados**
   ```bash
   make clean
   ```

---

## 🧠 Detalles técnicos

- Se utiliza **programación concurrente** con hilos POSIX (`pthread`).
- El acceso a datos compartidos (posición, estado, etc.) se controla con un **mutex global**.
- La lectura del archivo `config.txt` se realiza mediante funciones simples con `fscanf`.
- La simulación finaliza cuando:
  - Todos los héroes mueren, o  
  - Todos escapan del mapa.

---

## 🧩 Ejemplo de configuración

```txt
GRID_SIZE 10 10
HERO_COUNT 1
MONSTER_COUNT 1
HERO 1 HP 100
HERO 1 ATTACK_DAMAGE 20
HERO 1 ATTACK_RANGE 1
HERO 1 START 0 0
HERO 1 PATH (0,0) (1,0) (2,0)
MONSTER 1 HP 50
MONSTER 1 ATTACK_DAMAGE 10
MONSTER 1 VISION_RANGE 3
MONSTER 1 ATTACK_RANGE 1
MONSTER 1 COORDS 2 0
```

---

## 🎯 Objetivo del proyecto
Aplicar los conocimientos vistos en clases sobre:
- **Procesos e hilos**
- **Sincronización y exclusión mutua**
- **Diseño modular en C**

El programa busca demostrar comprensión práctica de estos temas mediante un entorno simulado y controlado.

---

