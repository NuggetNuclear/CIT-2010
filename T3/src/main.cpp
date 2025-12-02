#include <iostream>
#include <vector>
#include <map>       // Usar map para gestionar procesos por ID
#include <random>    // Para generación de números aleatorios
#include <chrono>    // Para manejo de tiempo de alta resolución
#include <thread>    // Para manejo de hilos y pausas
#include <iomanip>   // Para manipulación de salida
#include <algorithm> // Para algoritmos como sort, find, etc.
#include <limits>    // Para límites de tipos de datos
#include <climits>   // Para constantes de límites de tipos de datos

using namespace std;

// Códigos de color ANSI para terminal
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"
#define BOLD "\033[1m"

// Estructura para representar una página
struct Pagina
{
    int procesoId;
    int numeroPagina;
    bool enRAM;
    int marco;                       // marco en RAM (-1 si está en swap)
    unsigned long long ultimoAcceso; // Contador de accesos en lugar de tiempo

    Pagina(int pid, int numPag) : procesoId(pid), numeroPagina(numPag),
                                  enRAM(false), marco(-1), ultimoAcceso(0) {}
};

struct Proceso
{
    int id;
    int tamanio;
    int numeroPaginas;
    vector<Pagina *> paginas;
    time_t tiempoCreacion;

    Proceso(int pid, int tam, int tamPagina) : id(pid), tamanio(tam),
                                               tiempoCreacion(time(0))
    {
        numeroPaginas = (tamanio * 1024) / tamPagina; // convertir MB a KB
        if ((tamanio * 1024) % tamPagina != 0)
            numeroPaginas++;
    }
};

class SimuladorPaginacion
{
private:
    int memoriaFisicaMB;
    int memoriaFisicaKB;
    int memoriaVirtualKB;
    int tamanioPagina;
    int numMarcosRAM;
    int numMarcosSwap;
    int minTamProceso;
    int maxTamProceso;

    vector<Pagina *> RAM;
    vector<Pagina *> SWAP;
    map<int, Proceso *> procesos;

    int contadorProcesos;
    int pageFaults;
    unsigned long long contadorReloj; // Contador global para LRU preciso

    random_device rd;
    mt19937 gen;

public:
    SimuladorPaginacion(int memFisica, int tamPag, int minProc, int maxProc)
        : memoriaFisicaMB(memFisica), tamanioPagina(tamPag),
          minTamProceso(minProc), maxTamProceso(maxProc),
          contadorProcesos(0), pageFaults(0), contadorReloj(0), gen(rd())
    {

        //  Parte 1: Configurar memoria física y virtual -----------------------

        memoriaFisicaKB = memoriaFisicaMB * 1024;

        // Generar tamaño de memoria virtual (1.5 a 4.5 veces la física) -------

        uniform_real_distribution<> dist(1.5, 4.5);
        double factor = dist(gen);
        memoriaVirtualKB = (int)(memoriaFisicaKB * factor);

        numMarcosRAM = memoriaFisicaKB / tamanioPagina;
        numMarcosSwap = (memoriaVirtualKB - memoriaFisicaKB) / tamanioPagina;

        RAM.resize(numMarcosRAM, nullptr);
        SWAP.resize(numMarcosSwap, nullptr);

        cout << BOLD << CYAN << "\n========== CONFIGURACIÓN DEL SISTEMA ==========" << RESET << endl;
        cout << GREEN << "Memoria Física: " << RESET << memoriaFisicaMB << " MB (" << memoriaFisicaKB << " KB)" << endl;
        cout << BLUE << "Memoria Virtual: " << RESET << (memoriaVirtualKB / 1024) << " MB (" << memoriaVirtualKB << " KB)" << endl;
        cout << "Tamaño de Página: " << tamanioPagina << " KB" << endl;
        cout << GREEN << "Marcos en RAM: " << RESET << numMarcosRAM << endl;
        cout << BLUE << "Marcos en SWAP: " << RESET << numMarcosSwap << endl;
        cout << "Rango de tamaño de procesos: " << minTamProceso << "-" << maxTamProceso << " MB" << endl;
        cout << YELLOW << "Política de reemplazo: LRU (Least Recently Used)" << RESET << endl;
        cout << BOLD << CYAN << "===============================================\n" << RESET << endl;
    }

    ~SimuladorPaginacion() // DESTRUIIIRRRRRRRRRR RAHHHH
    {
        for (auto &par : procesos)
        {
            for (auto pagina : par.second->paginas)
            {
                delete pagina;
            }
            delete par.second;
        }
    }

    // Parte 2: Gestión de procesos y páginas
    
    bool crearProceso()
    {
        uniform_int_distribution<> dist(minTamProceso, maxTamProceso);
        int tamanio = dist(gen);

        contadorProcesos++;
        Proceso *proc = new Proceso(contadorProcesos, tamanio, tamanioPagina);

        cout << GREEN << "[CREAR] " << RESET << "Proceso " << proc->id << " creado. Tamaño: "
             << proc->tamanio << " MB, Páginas: " << proc->numeroPaginas << endl;

        // Intentar asignar páginas
        int paginasEnRAM = 0;
        int paginasEnSwap = 0;

        for (int i = 0; i < proc->numeroPaginas; i++)
        {
            Pagina *pag = new Pagina(proc->id, i);
            proc->paginas.push_back(pag);

            // Intentar colocar en RAM primero
            int marcoLibre = buscarMarcoLibre(RAM);
            if (marcoLibre != -1)
            {
                RAM[marcoLibre] = pag;
                pag->enRAM = true;
                pag->marco = marcoLibre;
                paginasEnRAM++;
            }
            else
            {
                // Colocar en SWAP
                int marcoSwap = buscarMarcoLibre(SWAP);
                if (marcoSwap != -1)
                {
                    SWAP[marcoSwap] = pag;
                    pag->enRAM = false;
                    pag->marco = marcoSwap;
                    paginasEnSwap++;
                }
                else
                {
                    cout << RED << "\n[ERROR] No hay memoria disponible (RAM ni SWAP). Finalizando simulación..." << RESET << endl;

                    // Limpiar correctamente las páginas ya asignadas
                    for (int j = 0; j < i; j++)
                    {
                        Pagina *p = proc->paginas[j];
                        if (p->enRAM)
                        {
                            RAM[p->marco] = nullptr;
                        }
                        else
                        {
                            SWAP[p->marco] = nullptr;
                        }
                        delete p;
                    }
                    delete pag;
                    delete proc;
                    return false;
                }
            }
        }

        if (paginasEnSwap > 0)
        {
            cout << YELLOW << "   → " << paginasEnRAM << " páginas en RAM, "
                 << paginasEnSwap << " páginas en SWAP" << RESET << endl;
        }

        procesos[proc->id] = proc;
        mostrarEstadoMemoria();
        return true;
    }

    // Finaliza un proceso aleatorio

    void finalizarProcesoAleatorio() 
    {
        if (procesos.empty())
            return;

        uniform_int_distribution<> dist(0, procesos.size() - 1);
        auto it = procesos.begin();
        advance(it, dist(gen));

        int pid = it->first;
        Proceso *proc = it->second;

        cout << MAGENTA << "\n[FINALIZAR] " << RESET << "Proceso " << pid << " finalizando..." << endl;

        // Liberar todas las páginas del proceso
        for (auto pagina : proc->paginas)
        {
            if (pagina->enRAM)
            {
                RAM[pagina->marco] = nullptr;
            }
            else
            {
                SWAP[pagina->marco] = nullptr;
            }
            delete pagina;
        }

        delete proc;
        procesos.erase(pid);

        cout << MAGENTA << "[FINALIZAR] " << RESET << "Proceso " << pid << " finalizado. Memoria liberada." << endl;
        mostrarEstadoMemoria();
    }

    void accederDireccionVirtual()
    {
        if (procesos.empty())
        {
            cout << YELLOW << "\n[ACCESO] No hay procesos activos para acceder." << RESET << endl;
            return;
        }

        // Seleccionar proceso aleatorio
        uniform_int_distribution<> distProc(0, procesos.size() - 1);
        auto it = procesos.begin();
        advance(it, distProc(gen));
        Proceso *proc = it->second;

        // Generar número de página aleatorio
        uniform_int_distribution<> distPag(0, proc->numeroPaginas - 1);
        int numPagina = distPag(gen);

        int direccionVirtual = (proc->id * 10000) + (numPagina * tamanioPagina);

        cout << CYAN << "\n[ACCESO] " << RESET << "Accediendo a dirección virtual: " << BOLD << direccionVirtual << RESET
             << " (Proceso " << proc->id << ", Página " << numPagina << ")" << endl;

        Pagina *pagina = proc->paginas[numPagina];

        if (pagina->enRAM)
        {
            cout << GREEN << "[ACCESO] Página encontrada en RAM (Marco " << pagina->marco << "). HIT!" << RESET << endl;
            pagina->ultimoAcceso = ++contadorReloj; // Actualizar con contador
        }
        else
        {
            cout << RED << "[ACCESO] Página NO encontrada en RAM. PAGE FAULT!" << RESET << endl;
            pageFaults++;

            // IMPORTANTE: Guardar el marco SWAP de la página entrante ANTES de buscar espacio
            int marcoSwapOrigen = pagina->marco;

            // Buscar marco libre en RAM
            int marcoLibre = buscarMarcoLibre(RAM);

            if (marcoLibre == -1)
            {
                // No hay marco libre - Aplicar política LRU
                cout << YELLOW << "[SWAP] No hay marcos libres. Aplicando política LRU..." << RESET << endl;
                marcoLibre = aplicarLRU();

                if (marcoLibre == -1)
                {
                    cout << RED << "[ERROR] LRU no pudo encontrar marco víctima. Finalizando..." << RESET << endl;
                    throw runtime_error("LRU falló");
                }

                Pagina *paginaVictima = RAM[marcoLibre];

                cout << YELLOW << "[SWAP] Víctima seleccionada: Proceso " << paginaVictima->procesoId
                     << ", Página " << paginaVictima->numeroPagina << " (menos recientemente usada)" << RESET << endl;

                // CORRECCIÓN CRÍTICA: Liberar primero el espacio de la página entrante en SWAP
                SWAP[marcoSwapOrigen] = nullptr;

                // Ahora buscar espacio para la víctima (encontrará al menos el hueco que dejamos)
                int marcoSwap = buscarMarcoLibre(SWAP);
                if (marcoSwap == -1)
                {
                    cout << RED << "[ERROR] No hay espacio en SWAP después de liberar. Finalizando..." << RESET << endl;
                    throw runtime_error("Sin espacio en SWAP");
                }

                SWAP[marcoSwap] = paginaVictima;
                paginaVictima->enRAM = false;
                paginaVictima->marco = marcoSwap;

                cout << BLUE << "[SWAP] Página víctima movida a SWAP (Marco " << marcoSwap << ")" << RESET << endl;
            }
            else
            {
                cout << GREEN << "[SWAP] Marco libre encontrado (Marco " << marcoLibre << "). No se requiere reemplazo." << RESET << endl;
                // Liberar el marco SWAP de la página entrante
                SWAP[marcoSwapOrigen] = nullptr;
            }

            // Traer página a RAM
            RAM[marcoLibre] = pagina;
            pagina->enRAM = true;
            pagina->marco = marcoLibre;
            pagina->ultimoAcceso = ++contadorReloj; // Actualizar con contador

            cout << BLUE << "[SWAP] Página solicitada cargada en RAM (Marco " << marcoLibre << ")" << RESET << endl;
        }
    }

    void mostrarEstadoMemoria()
    {
        int paginasEnRAM = 0, paginasEnSwap = 0;

        for (auto pag : RAM)
        {
            if (pag != nullptr)
                paginasEnRAM++;
        }

        for (auto pag : SWAP)
        {
            if (pag != nullptr)
                paginasEnSwap++;
        }

        cout << "\n--- Estado de Memoria ---" << endl;
        cout << "Procesos activos: " << procesos.size() << endl;
        cout << GREEN << "RAM: " << RESET << paginasEnRAM << "/" << numMarcosRAM << " marcos ocupados ("
             << fixed << setprecision(1) << (paginasEnRAM * 100.0 / numMarcosRAM) << "%)" << endl;
        cout << BLUE << "SWAP: " << RESET << paginasEnSwap << "/" << numMarcosSwap << " marcos ocupados ("
             << fixed << setprecision(1) << (paginasEnSwap * 100.0 / numMarcosSwap) << "%)" << endl;
        cout << "Page Faults totales: " << pageFaults << endl;
        cout << "-------------------------\n"
             << endl;
    }

    void ejecutarSimulacion()
    {
        cout << BOLD << GREEN << "\n========== INICIANDO SIMULACIÓN ==========" << RESET << endl;
        auto inicio = chrono::steady_clock::now();

        try
        {
            while (true)
            {
                auto ahora = chrono::steady_clock::now();
                auto duracion = chrono::duration_cast<chrono::seconds>(ahora - inicio).count();

                cout << BOLD << "\n>>> Tiempo transcurrido: " << duracion << " segundos <<<" << RESET << endl;

                // Crear proceso cada 2 segundos
                if (duracion % 2 == 0)
                {
                    if (!crearProceso())
                    {
                        break;
                    }
                }

                // A partir de los 30 segundos
                if (duracion >= 30)
                {
                    // Finalizar proceso cada 5 segundos
                    if (duracion % 5 == 0)
                    {
                        finalizarProcesoAleatorio();
                        this_thread::sleep_for(chrono::milliseconds(500));
                        accederDireccionVirtual();
                    }
                }

                this_thread::sleep_for(chrono::seconds(1));
            }
        }
        catch (const exception &e)
        {
            cout << RED << "\n[ERROR] " << e.what() << RESET << endl;
        }

        cout << BOLD << RED << "\n========== SIMULACIÓN FINALIZADA ==========" << RESET << endl;
        mostrarEstadoMemoria();
        cout << "\nEstadísticas finales:" << endl;
        cout << "Total de procesos creados: " << contadorProcesos << endl;
        cout << "Total de page faults: " << pageFaults << endl;
        cout << "===========================================" << endl;
    }

private:
    int buscarMarcoLibre(vector<Pagina *> &memoria)
    {
        for (size_t i = 0; i < memoria.size(); i++)
        {
            if (memoria[i] == nullptr)
            {
                return i;
            }
        }
        return -1;
    }

    int aplicarLRU()
    {
        // Encuentra la página con el acceso más antiguo
        int marcoVictima = -1;
        unsigned long long tiempoMasAntiguo = ULLONG_MAX;

        for (size_t i = 0; i < RAM.size(); i++)
        {
            if (RAM[i] != nullptr && RAM[i]->ultimoAcceso < tiempoMasAntiguo)
            {
                tiempoMasAntiguo = RAM[i]->ultimoAcceso;
                marcoVictima = i;
            }
        }

        return marcoVictima;
    }
};

int main()
{
    cout << BOLD << CYAN << "========================================" << RESET << endl;
    cout << BOLD << CYAN << "  SIMULADOR DE PAGINACIÓN DE MEMORIA" << RESET << endl;
    cout << BOLD << CYAN << "  Sistemas Operativos - Tarea 3" << RESET << endl;
    cout << BOLD << CYAN << "========================================\n"
         << RESET << endl;

    int memoriaFisica, tamanioPagina, minTamProceso, maxTamProceso;

    cout << "Ingrese el tamaño de la memoria física (en MB): ";
    cin >> memoriaFisica;

    cout << "Ingrese el tamaño de cada página (en KB): ";
    cin >> tamanioPagina;

    cout << "Ingrese el tamaño mínimo de proceso (en MB): ";
    cin >> minTamProceso;

    cout << "Ingrese el tamaño máximo de proceso (en MB): ";
    cin >> maxTamProceso;

    if (memoriaFisica <= 0 || tamanioPagina <= 0 || minTamProceso <= 0 ||
        maxTamProceso < minTamProceso)
    {
        cout << RED << "\n[ERROR] Parámetros inválidos." << RESET << endl;
        return 1;
    }

    SimuladorPaginacion simulador(memoriaFisica, tamanioPagina, minTamProceso, maxTamProceso);
    simulador.ejecutarSimulacion();

    return 0;
}