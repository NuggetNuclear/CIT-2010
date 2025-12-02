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

## 🧩 Ejemplo de configuración 1

```txt
GRID_SIZE 30 20  
HERO_HP 150  
HERO_ATTACK_DAMAGE 20  
HERO_ATTACK_RANGE 3  
HERO_START 2 2  
HERO_PATH (3,2) (4,2) (5,2) (5,3) (5,4) (6,4)  

MONSTER_COUNT 3  
MONSTER_1_HP 50  
MONSTER_1_ATTACK_DAMAGE 10  
MONSTER_1_VISION_RANGE 5  
MONSTER_1_ATTACK_RANGE 1  
MONSTER_1_COORDS 8 4  

MONSTER_2_HP 50  
MONSTER_2_ATTACK_DAMAGE 10  
MONSTER_2_VISION_RANGE 5  
MONSTER_2_ATTACK_RANGE 1  
MONSTER_2_COORDS 15 10  

MONSTER_3_HP 80  
MONSTER_3_ATTACK_DAMAGE 15  
MONSTER_3_VISION_RANGE 4  
MONSTER_3_ATTACK_RANGE 2  
MONSTER_3_COORDS 5 8
```
## 🧩 Ejemplo de configuración 2
```txt
GRID_SIZE 40 20
MONSTER_COUNT 2

HERO 1 HP 100
HERO 1 ATTACK_DAMAGE 20
HERO 1 ATTACK_RANGE 3
HERO 1 START 2 2
HERO 1 PATH (3,2) (4,2) (5,2) (5,3) (5,4)

HERO 2 HP 120
HERO 2 ATTACK_DAMAGE 25
HERO 2 ATTACK_RANGE 2
HERO 2 START 10 2
HERO 2 PATH (11,2) (12,2) (13,2) (14,3) (15,3)

MONSTER 1 HP 60
MONSTER 1 ATTACK_DAMAGE 10
MONSTER 1 VISION_RANGE 5
MONSTER 1 ATTACK_RANGE 1
MONSTER 1 COORDS 8 4

MONSTER 2 HP 80
MONSTER 2 ATTACK_DAMAGE 15
MONSTER 2 VISION_RANGE 6
MONSTER 2 ATTACK_RANGE 2
MONSTER 2 COORDS 20 10
```
## 🧩 Ejemplo de configuración 3
```txt
GRID_SIZE 25 15
MONSTER_COUNT 2

HERO_1_HP 150
HERO_1_ATTACK_DAMAGE 25
HERO_1_ATTACK_RANGE 3
HERO_1_START 1 1
HERO_1_PATH (2,1) (3,1) (4,1) (4,2) (4,3)

HERO_2_HP 130
HERO_2_ATTACK_DAMAGE 20
HERO_2_ATTACK_RANGE 2
HERO_2_START 10 3
HERO_2_PATH (11,3) (12,3) (13,3) (14,4)

MONSTER_1_HP 70
MONSTER_1_ATTACK_DAMAGE 10
MONSTER_1_VISION_RANGE 4
MONSTER_1_ATTACK_RANGE 1
MONSTER_1_COORDS 7 3

MONSTER_2_HP 80
MONSTER_2_ATTACK_DAMAGE 12
MONSTER_2_VISION_RANGE 5
MONSTER_2_ATTACK_RANGE 2
MONSTER_2_COORDS 15 5
```

---

## 🎯 Objetivo del proyecto
Aplicar los conocimientos vistos en clases sobre:
- **Procesos e hilos**
- **Sincronización y exclusión mutua**
- **Diseño modular en C**

El programa busca demostrar comprensión práctica de estos temas mediante un entorno simulado y controlado.

---

