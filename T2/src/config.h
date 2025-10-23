#ifndef CONFIG_H
#define CONFIG_H

#include <pthread.h>

#define MAX_PATH 100
#define MAX_MONSTERS 20
#define MAX_HEROES 5 // Para N Heroes

//Cada struct guarda un grupo de datos relacionados.

typedef struct {
    int x;
    int y;
} Point;

typedef struct {
    int id; 
    int hp;
    int attack;
    int range;
    Point start;
    Point path[MAX_PATH];
    int path_len;
    Point posActual;
    
    int en_combate; 
    int path_step;
    int path_finished;

} Hero;

typedef struct {
    int id;
    int hp;
    int attack;
    int vision;
    int range;
    Point pos;
    int alertado; // 0 = dormido, 1 = alerta
    int target_hero_id; // ID del heroe que persigue (-1 = ninguno)
} Monster;

typedef struct {
    int width;
    int height;
    
    Hero heroes[MAX_HEROES]; // Array de Heroes
    int hero_count;
    
    Monster monsters[MAX_MONSTERS];
    int monster_count;

    // Sincronizacion y estado global
    pthread_mutex_t mutex; 
    int juegoActivo;

} GameConfig;
//GameConfig agrupa todo (heroes, monstruos, mapa y mutex).

int leerConfig(const char *nombreArchivo, GameConfig *cfg);
//este lee full el txt
#endif