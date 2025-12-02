# Sistemas Operativos — Simulador de Paginación de Memoria (T3)

**Plataforma:** Linux/UNIX (probado en entorno tipo Ubuntu)  
**Lenguaje:** C++17  
**Archivo único de código:** `main.cpp`  

---

## 1. Descripción general

Este programa implementa un **simulador de paginación de memoria** a nivel lógico, donde se modelan:

- **Memoria física (RAM)** y **memoria virtual (SWAP)** en marcos de página.
- **Procesos lógicos** que ocupan una cierta cantidad de páginas.
- **Accesos a direcciones virtuales**, produciendo **HIT** o **PAGE FAULT**.
- Un mecanismo de **swap** con política de reemplazo **LRU (Least Recently Used)**.

El objetivo es emular el comportamiento descrito en el enunciado de la Tarea 3: creación periódica de procesos, liberación aleatoria de procesos, accesos a memoria virtual y finalización del programa cuando no queda espacio disponible ni en RAM ni en SWAP.

Los mensajes se muestran en consola con **colores ANSI** para facilitar seguir el flujo (creación, acceso, swap, finalización, errores).

---

## 2. Requisitos

- **Sistema operativo:** Linux/UNIX o WSL (Ubuntu recomendado).
- **Compilador:** `g++` con soporte **C++17**.
- Consola/terminal que soporte **códigos de color ANSI** (la mayoría de terminales en Linux).

---

## 3. Compilación

Desde la carpeta donde está `main.cpp`:

```bash
g++ -std=c++17 main.cpp -o simulador_memoria
```

> No se usan librerías externas; solo la STL de C++.

---

## 4. Ejecución

Desde la misma carpeta:

```bash
./simulador_memoria
```

El programa pedirá por entrada estándar:

1. `memoriaFisica` – tamaño de la memoria física **en MB** (entero > 0).
2. `tamanioPagina` – tamaño de cada página **en KB** (entero > 0).
3. `minTamProceso` – tamaño mínimo de proceso **en MB** (entero > 0).
4. `maxTamProceso` – tamaño máximo de proceso **en MB** (entero ≥ minTamProceso).

Si algún parámetro es inválido, el programa termina con:

```text
[ERROR] Parámetros inválidos.
```

Con parámetros válidos, se muestra una **configuración inicial** donde se informa:

- Memoria física total en MB y KB.
- Memoria virtual generada aleatoriamente entre **1.5x y 4.5x** la memoria física.
- Tamaño de página.
- Cantidad de marcos en RAM y en SWAP.
- Rango de tamaño de procesos.
- Política de reemplazo: **LRU (Least Recently Used)**.

Luego comienza la simulación.

---

## 5. Parámetros de entrada y modelo de memoria

### 5.1. Memoria física y virtual

- `memoriaFisicaMB` se transforma a KB:
  ```cpp
  memoriaFisicaKB = memoriaFisicaMB * 1024;
  ```
- `memoriaVirtualKB` se genera como:
  ```cpp
  factor ∈ [1.5, 4.5]  // aleatorio, distribución uniforme
  memoriaVirtualKB = memoriaFisicaKB * factor;
  ```
- **Marcos de RAM**:
  ```cpp
  numMarcosRAM = memoriaFisicaKB / tamanioPagina;
  ```
- **Marcos de SWAP**:
  ```cpp
  numMarcosSwap = (memoriaVirtualKB - memoriaFisicaKB) / tamanioPagina;
  ```

La memoria se representa con:

- `vector<Pagina*> RAM;`  de tamaño `numMarcosRAM`.
- `vector<Pagina*> SWAP;` de tamaño `numMarcosSwap`.

Cada posición del vector es **un marco** que puede estar vacío (`nullptr`) o apuntar a una `Pagina`.

---

## 6. Arquitectura de datos

### 6.1. Estructura `Pagina`

```cpp
struct Pagina {
    int procesoId;             // ID lógico del proceso dueño
    int numeroPagina;          // Índice de página dentro del proceso
    bool enRAM;                // true si está en RAM, false si está en SWAP
    int marco;                 // Índice de marco (en RAM o en SWAP)
    unsigned long long ultimoAcceso; // contador global para LRU

    Pagina(int pid, int numPag);
};
```

### 6.2. Estructura `Proceso`

```cpp
struct Proceso {
    int id;
    int tamanio;               // tamaño en MB
    int numeroPaginas;
    vector<Pagina*> paginas;   // todas las páginas del proceso
    time_t tiempoCreacion;

    Proceso(int pid, int tam, int tamPagina);
};
```

El número de páginas de un proceso se calcula como:

```cpp
numeroPaginas = ceil( (tamanio_MB * 1024 /* KB */) / tamanioPaginaKB );
```

### 6.3. Clase `SimuladorPaginacion`

A grandes rasgos, la clase mantiene:

- Parámetros de configuración.
- Vectores `RAM` y `SWAP`.
- `map<int, Proceso*> procesos;` para tener acceso directo a cada proceso por su ID.
- Contadores:
  - `contadorProcesos` – cuántos procesos se han creado.
  - `pageFaults` – número total de page faults.
  - `contadorReloj` – contador global, usado para LRU.

Principales métodos públicos:

- `bool crearProceso();`
- `void finalizarProcesoAleatorio();`
- `void accederDireccionVirtual();`
- `void mostrarEstadoMemoria();`
- `void ejecutarSimulacion();`

---

## 7. Flujo temporal de la simulación

La simulación se controla con `chrono::steady_clock` y un loop principal:

```cpp
auto inicio = chrono::steady_clock::now();
while (true) {
    auto ahora = chrono::steady_clock::now();
    auto duracion = chrono::duration_cast<chrono::seconds>(ahora - inicio).count();

    cout << ">>> Tiempo transcurrido: " << duracion << " segundos <<<" << endl;
    ...
    this_thread::sleep_for(chrono::seconds(1));
}
```

Dentro del loop:

1. **Cada 1 segundo**  
   Se imprime el tiempo transcurrido.

2. **Cada 2 segundos (`duracion % 2 == 0`)**  
   Se llama a `crearProceso()`.  
   - El tamaño del proceso se elige aleatoriamente en el rango `[minTamProceso, maxTamProceso]`.
   - Se calculan sus páginas y se intenta asignarlas primero en RAM, luego en SWAP.  
   - Si en algún punto no hay ni marcos libres en RAM ni en SWAP, se muestra un mensaje de error y se retorna `false`, lo que hace que el loop principal termine.

3. **A partir de los 30 segundos (`duracion >= 30`)**  
   Cada **5 segundos**, es decir cuando `duracion % 5 == 0`:
   - Se llama a `finalizarProcesoAleatorio();`
   - Se espera 500 ms (`sleep_for(chrono::milliseconds(500))`) para que el output sea legible.
   - Se llama a `accederDireccionVirtual();` para simular un acceso a memoria virtual.

Cuando el bucle termina (ya sea por falta de memoria o por una excepción de error en swap), se imprime:

- `========== SIMULACIÓN FINALIZADA ==========`  
- Estado final de memoria (RAM/SWAP/page faults).

---

## 8. Creación y finalización de procesos (Ítem 7)

### 8.1. Creación (`crearProceso()`)

1. Se genera un tamaño al azar en MB dentro del rango definido.
2. Se crea un objeto `Proceso` con un nuevo ID.
3. Se calculan las páginas requeridas.
4. Por cada página:
   - Se intenta buscar un marco libre en **RAM** (`buscarMarcoLibre(RAM)`).
   - Si hay marco libre en RAM:
     - Se asigna la página a ese marco (`enRAM = true`, `marco = índice`).
   - Si no hay marco libre en RAM:
     - Se busca un marco libre en **SWAP**.
     - Si hay marco libre en SWAP:
       - Se asigna allí (`enRAM = false`, `marco = índiceSwap`).
     - Si tampoco hay espacio en SWAP:
       - Se imprime un mensaje de error:
         ```text
         [ERROR] No hay memoria disponible (RAM ni SWAP). Finalizando simulación...
         ```
       - Se **liberan todas las páginas ya asignadas** a este proceso (tanto en RAM como en SWAP).
       - Se destruye el proceso y la función devuelve `false`.

Después de asignar todas las páginas, se guarda el proceso en el `map<int, Proceso*> procesos` y se imprime el estado de memoria.

### 8.2. Finalización (`finalizarProcesoAleatorio()`)

1. Si no hay procesos activos, la función termina.
2. Se elige un proceso al azar desde `procesos`.
3. Se imprime:
   ```text
   [FINALIZAR] Proceso X finalizando...
   ```
4. Para cada página del proceso:
   - Si está en RAM, se limpia su marco en `RAM`.
   - Si está en SWAP, se limpia su marco en `SWAP`.
   - Se libera la memoria (`delete pagina`).
5. Se libera el objeto `Proceso` y se borra del `map`.
6. Se imprime:
   ```text
   [FINALIZAR] Proceso X finalizado. Memoria liberada.
   ```
7. Se muestra nuevamente el estado de memoria.

Esto garantiza que **el espacio asignado a los procesos se libera correctamente**, cumpliendo el ítem 7 de la rúbrica.

---

## 9. Acceso a direcciones virtuales y page faults (Ítems 2 y 3)

El acceso se simula en `accederDireccionVirtual()`:

1. Si no hay procesos activos, se imprime un aviso y se retorna.
2. Se elige **un proceso aleatorio** desde el `map`.
3. Se elige **un número de página aleatorio** dentro de ese proceso.
4. Se construye una dirección virtual simplificada:
   ```cpp
   direccionVirtual = (proc->id * 10000) + (numPagina * tamanioPagina);
   ```
   Esto no es una dirección real, sino un identificador “bonito” para mostrar en pantalla qué proceso y página se están accediendo.

5. Se imprime algo como:
   ```text
   [ACCESO] Accediendo a dirección virtual: XXXXX (Proceso P, Página N)
   ```

6. Se toma la `Pagina*` correspondiente:
   ```cpp
   Pagina* pagina = proc->paginas[numPagina];
   ```

7. Si `pagina->enRAM == true`:
   - Se imprime un **HIT**:
     ```text
     [ACCESO] Página encontrada en RAM (Marco X). HIT!
     ```
   - Se actualiza `pagina->ultimoAcceso = ++contadorReloj;`

8. Si la página **no está en RAM**:
   - Se imprime:
     ```text
     [ACCESO] Página NO encontrada en RAM. PAGE FAULT!
     ```
   - Se incrementa `pageFaults`.
   - Se inicia el flujo de **swap** con la política LRU (sección siguiente).

Con esto, el programa **localiza correctamente la página**, produce page faults cuando corresponde y registra el total de fallos para el resumen final.

---

## 10. Política de reemplazo de páginas: LRU (Ítem 2)

La política de reemplazo utilizada es **LRU (Least Recently Used)** implementada con un **contador global** `contadorReloj`:

- Cada vez que una página es accedida (HIT) o traída a RAM después de un PAGE FAULT:
  ```cpp
  pagina->ultimoAcceso = ++contadorReloj;
  ```

- La función `aplicarLRU()` recorre todos los marcos de RAM:
  ```cpp
  int aplicarLRU() {
      int marcoVictima = -1;
      unsigned long long tiempoMasAntiguo = ULLONG_MAX;

      for (size_t i = 0; i < RAM.size(); i++) {
          if (RAM[i] != nullptr && RAM[i]->ultimoAcceso < tiempoMasAntiguo) {
              tiempoMasAntiguo = RAM[i]->ultimoAcceso;
              marcoVictima = i;
          }
      }
      return marcoVictima;
  }
  ```

- **Página víctima:** es la que tiene el menor valor de `ultimoAcceso`, es decir, la que fue usada hace más tiempo.

### 10.1. Flujo completo de un PAGE FAULT

Cuando una página está en SWAP (no en RAM) y es accedida:

1. Se guarda el **marco actual en SWAP** de la página entrante:
   ```cpp
   int marcoSwapOrigen = pagina->marco;
   ```
2. Se busca un marco libre en RAM.
3. Si hay marco libre:
   - Se imprime que se encontró un marco libre.
   - Se libera el marco antiguo en SWAP (la página que viene ya no estará en SWAP).
4. Si **no hay marcos libres en RAM**:
   - Se anuncia que se aplicará LRU.
   - `aplicarLRU()` devuelve el índice del marco víctima.
   - Se obtiene la `Pagina* paginaVictima = RAM[marcoLibre];`
   - Se **libera primero** el marco en SWAP de la página entrante:
     ```cpp
     SWAP[marcoSwapOrigen] = nullptr;
     ```
   - Luego se busca un marco libre en SWAP para la víctima.
     - Si no hay marco libre en SWAP, se lanza una excepción con mensaje de error y se termina la simulación.
   - Se mueve la página víctima a ese marco de SWAP y se actualizan sus campos.

5. Finalmente:
   - Se coloca la página solicitada en el marco libre de RAM.
   - Se marca `enRAM = true`, se actualiza su `marco`, y `ultimoAcceso = ++contadorReloj;`
   - Se imprime:
     ```text
     [SWAP] Página solicitada cargada en RAM (Marco X)
     ```

Esta lógica cumple el requerimiento de **simular swap con una política de reemplazo explícita y bien definida**.

---

## 11. Finalización del programa por falta de memoria (Ítem 5)

El programa puede terminar por dos motivos principales:

1. **Durante la creación de procesos** (`crearProceso()`):
   - Si al intentar asignar páginas de un nuevo proceso **no queda espacio en RAM ni en SWAP**, se imprime:
     ```text
     [ERROR] No hay memoria disponible (RAM ni SWAP). Finalizando simulación...
     ```
   - Se limpian todas las páginas que ya habían sido asignadas a ese proceso.
   - `crearProceso()` retorna `false` y el loop principal sale.

2. **Durante un PAGE FAULT en `accederDireccionVirtual()`**:
   - Si, al mover una página víctima a SWAP, **no se encuentra espacio en SWAP** aun después de liberar el marco de la página entrante:
     ```text
     [ERROR] No hay espacio en SWAP después de liberar. Finalizando...
     ```
   - Se lanza una excepción `runtime_error`.
   - Esta es capturada en `ejecutarSimulacion()`, donde se imprime `[ERROR] ...` y luego se imprime el resumen final.

En ambos casos, al terminar la simulación se muestra:

- Estado final de RAM y SWAP.
- Total de procesos creados.
- Total de page faults acumulados.

---

## 12. Salida en consola y legibilidad (Ítem 8)

El programa usa prefijos y colores para hacer la salida clara:

- `[CREAR]` en **verde**: creación de procesos.
- `[FINALIZAR]` en **magenta**: finalización de procesos y liberación de memoria.
- `[ACCESO]` en **cian**: accesos a direcciones virtuales (HIT/PAGE FAULT).
- `[SWAP]` en **amarillo/azul**: movimientos de páginas entre RAM y SWAP.
- `[ERROR]` en **rojo**: condiciones de error que provocan el fin de la simulación.

Además:

- Cada segundo se imprime un encabezado:
  ```text
  >>> Tiempo transcurrido: X segundos <<<
  ```
- Después de cada evento importante (creación, finalización, errores) se llama a:
  ```cpp
  mostrarEstadoMemoria();
  ```
  que muestra:
  - Número de procesos activos.
  - Marcos ocupados / totales en RAM y en SWAP (porcentaje incluido).
  - Total de page faults.

Esto permite seguir claramente el **flujo de ejecución** y visualizar cómo evoluciona la memoria.

---

## 13. Ejemplos de uso (escenarios sugeridos)

A modo de ejemplo, algunos parámetros útiles para pruebas:

### Escenario 1 – Memoria pequeña, mucha presión en SWAP

- Memoria física: `2 MB`
- Tamaño de página: `16 KB`
- Tamaño de procesos: mínimo `1 MB`, máximo `3 MB`

Aquí la RAM se llena muy rápido y los procesos nuevos empiezan a irse directo a SWAP hasta que se agota, disparando el mensaje de error y terminando la simulación.

### Escenario 2 – Memoria mayor, muchos procesos pequeños (escenario típico del video)

- Memoria física: `12 MB`
- Tamaño de página: `16 KB`
- Tamaño mínimo de proceso: `1 MB`
- Tamaño máximo de proceso: `1 MB`

En este caso:

- Se crean muchos procesos de 1 MB.
- La RAM se llena con 12 procesos (cada uno 64 páginas).
- Los procesos adicionales se empiezan a alojar completamente en SWAP.
- A partir de los 30 segundos se ven:
  - Finalización aleatoria de procesos.
  - Accesos a páginas provocando **HIT** y **PAGE FAULT**.
  - Uso de la política LRU cuando RAM está llena.
- Finalmente, cuando no queda espacio ni en RAM ni en SWAP, la simulación termina con un mensaje claro.

---

## Rúbrica

1. **Parámetros de memoria ingresados correctamente**  
   - Se pide tamaño de memoria física, tamaño de página y rango de tamaño de procesos.  
   - La memoria virtual se calcula aleatoriamente entre **1.5 y 4.5** veces la memoria física.

2. **Page faults y política de reemplazo**  
   - Cuando una página no está en RAM se produce un **PAGE FAULT** explícito.  
   - Se aplica una política de reemplazo **LRU**, implementada con `ultimoAcceso` y `contadorReloj`.

3. **Acceso periódico a direcciones virtuales**  
   - Desde el segundo 30, cada 5 segundos se genera un acceso a una dirección virtual aleatoria.  
   - Se muestra la dirección virtual, proceso, página, y si es HIT o PAGE FAULT.

4. **Liberación de procesos cada 5 segundos**  
   - Desde el segundo 30, cada 5 segundos se selecciona un proceso al azar y se finaliza, liberando todos sus marcos en RAM y SWAP.

5. **Terminación por falta de memoria clara**  
   - Si se queda sin marcos libres en RAM y SWAP (ya sea en creación o swap), se imprime un mensaje de error claro y se termina la simulación con un resumen.

6. **README**  
   - Este documento explica la implementación, el modelo de memoria, la política LRU y las instrucciones de compilación y ejecución.

7. **Simulación correcta de creación y finalización**  
   - Los procesos ocupan páginas en RAM o SWAP según disponibilidad.  
   - Al finalizar un proceso, se liberan correctamente todos sus marcos.

8. **Salida legible y flujo claro**  
   - Se usan colores, tags `[CREAR]`, `[FINALIZAR]`, `[ACCESO]`, `[SWAP]`, `[ERROR]`.  
   - Se imprimen estadísticas periódicas de RAM, SW