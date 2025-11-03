#ifndef CONFIG_H
#define CONFIG_H

#include <pthread.h>

#define MAX_PATH 200
#define MAX_MONSTERS 30
#define MAX_HEROES 5

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
    int escapado;
} Hero;

typedef struct {
    int id;
    int hp;
    int attack;
    int vision;
    int range;
    Point pos;
    int alertado;
    int target_hero_id;
} Monster;

typedef struct {
    int width;
    int height;
    Hero heroes[MAX_HEROES];
    int hero_count;
    Monster monsters[MAX_MONSTERS];
    int monster_count;
    pthread_mutex_t mutex;
    int juegoActivo;
} GameConfig;

int leerConfig(const char *nombreArchivo, GameConfig *cfg);

#endif
