# Tarea N° 2 - Simulación Doom 👾
**Sistemas Operativos**

Repositorio para la Tarea 2, una simulación de batalla concurrente implementada en C con Pthreads.

---

## 👥 Integrantes

- Matias Vigneau
- Gabriel Gonzalez

---

## ⚙️ Explicación del Funcionamiento

Este programa implementa una simulación concurrente donde N Héroes (N threads) y M Monstruos (M threads) interactúan en un mapa lógico. El objetivo es demostrar el manejo de concurrencia y sincronización.

El funcionamiento se basa en los siguientes pilares:

1. **Configuración Inicial:** El programa lee un archivo `config.txt` que define el tamaño del grid, la cantidad de héroes y monstruos, y todas sus estadísticas (HP, daño, rangos) y rutas.

2. **Concurrencia (Hilos):** Se crea un hilo (`pthread`) por cada entidad en el mapa (N Héroes + M Monstruos). Cada hilo ejecuta una función (`hiloHeroe` o `hiloMonstruo`) que define su comportamiento "IA" turno a turno.

3. **Sincronización (Mutex):** Para evitar *race conditions* (por ejemplo, que dos monstruos ataquen a un héroe al mismo tiempo y corrompan su HP), toda la simulación se sincroniza usando un único `pthread_mutex_t`. Cada hilo debe **bloquear** el mutex antes de leer o modificar datos compartidos (como el HP de otra entidad o imprimir en consola) y **liberarlo** después.

4. **Lógica del Héroe:**
   - El héroe sigue su `PATH` (ruta) predefinido paso a paso.
   - **Regla Crítica:** Si un monstruo entra en el `ATTACK_RANGE` del héroe, este **deja de moverse** y se queda en su posición actual para pelear. No retomará su camino hasta que todos los monstruos a su alrededor hayan muerto.

5. **Lógica del Monstruo:**
   - Inicia en estado pasivo.
   - Si un héroe entra en su `VISION_RANGE`, se "alerta" y comienza a perseguirlo (usando la ruta más corta de Manhattan).
   - **Alerta en Cadena:** En el momento en que un monstruo ve a un héroe, primero alerta a todos los *otros monstruos* que estén dentro de su propio rango de visión, "despertándolos" para que se unan al ataque.
   - Cuando el héroe está en `ATTACK_RANGE`, el monstruo ataca.

---

## 🚀 Cómo Compilar y Ejecutar

El proyecto incluye un `Makefile` para una gestión sencilla. Es requisito indispensable compilar y ejecutar en un **ambiente UNIX** (como Linux, WSL o macOS).

### 1. Compilar
Para compilar el programa y generar el ejecutable `simulador`, use el comando:

```bash
make
```

### 2. Ejecutar
Para correr la simulación (asegúrese de que `config.txt` esté presente):

```bash
make run
```

### 3. Limpiar
Para eliminar el archivo ejecutable (`simulador`):

```bash
make clean
```
