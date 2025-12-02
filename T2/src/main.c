#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <stdlib.h>
#include "config.h"

typedef struct { GameConfig* cfg; int id; } ArgsHeroe;
typedef struct { GameConfig* cfg; int id; } ArgsMonstruo;

int distanciaManhattan(Point a, Point b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}

static int heroesActivosCount(GameConfig* cfg) {
    int c = 0;
    for (int i = 0; i < cfg->hero_count; i++) {
        if (cfg->heroes[i].hp > 0 && !cfg->heroes[i].escapado) c++;
    }
    return c;
}

static int todosMuertos(GameConfig* cfg) {
    for (int i = 0; i < cfg->hero_count; i++)
        if (cfg->heroes[i].hp > 0) return 0;
    return 1;
}

static int todosEscapados(GameConfig* cfg) {
    for (int i = 0; i < cfg->hero_count; i++)
        if (!cfg->heroes[i].escapado) return 0;
    return 1;
}

static void actualizarEstadoJuego(GameConfig* cfg) {
    int activos = heroesActivosCount(cfg);
    if (activos == 0) {
        cfg->juegoActivo = 0;
        printf("=== No quedan heroes activos. Fin de la simulacion. ===\n");
        return;
    }
    if (todosMuertos(cfg)) {
        cfg->juegoActivo = 0;
        printf("=== Todos los heroes han muerto. Fin de la simulacion. ===\n");
        return;
    }
    if (todosEscapados(cfg)) {
        cfg->juegoActivo = 0;
        printf("=== Todos los heroes escaparon. Fin de la simulacion. ===\n");
        return;
    }
}

// alerta a otros monstruos cercanos si uno ve a un heroe
static void alertarMonstruos(GameConfig* cfg, int idx_monstruo_alerta, int id_heroe_visto) {
    Monster* m = &cfg->monsters[idx_monstruo_alerta];

    for (int i = 0; i < cfg->monster_count; i++) {
        if (i == idx_monstruo_alerta) continue;
        Monster* otro = &cfg->monsters[i];
        if (otro->hp <= 0 || otro->alertado) continue;
        if (cfg->heroes[id_heroe_visto].escapado) continue;

        int d = distanciaManhattan(m->pos, otro->pos);
        if (d <= m->vision) {
            otro->alertado = 1;
            otro->target_hero_id = id_heroe_visto;
            printf("[MONSTRUO %d] Alerta a MONSTRUO %d (Heroe %d)\n",
                   m->id, otro->id, id_heroe_visto + 1);
        }
    }
}

// hilo que controla el movimiento y ataques del heroe
void* hiloHeroe(void* arg) {
    ArgsHeroe* a = (ArgsHeroe*) arg;
    GameConfig* cfg = a->cfg;
    Hero* hero = &cfg->heroes[a->id];

    pthread_mutex_lock(&cfg->mutex);
    hero->posActual = hero->start;
    hero->path_step = 0;
    hero->path_finished = 0;
    hero->en_combate = 0;
    hero->escapado = 0;
    printf("[HEROE %d] Inicia en (%d,%d) con %d HP\n",
           hero->id, hero->posActual.x, hero->posActual.y, hero->hp);
    pthread_mutex_unlock(&cfg->mutex);

    while (1) {
        pthread_mutex_lock(&cfg->mutex);
        int juego = cfg->juegoActivo;
        int hpvivo = (hero->hp > 0);
        int escapado = hero->escapado;
        pthread_mutex_unlock(&cfg->mutex);
        if (!juego || !hpvivo || escapado) break;

        hero->en_combate = 0;
        Monster* monstruo_atacante = NULL;

        // combate
        pthread_mutex_lock(&cfg->mutex);
        for (int i = 0; i < cfg->monster_count; i++) {
            Monster* m = &cfg->monsters[i];
            if (m->hp <= 0) continue;
            int d = distanciaManhattan(hero->posActual, m->pos);
            if (d <= hero->range) {
                hero->en_combate = 1;
                monstruo_atacante = m;
                break;
            }
        }
        if (hero->en_combate && monstruo_atacante) {
            Monster* m = monstruo_atacante;
            printf("[HEROE %d] Ataca a MONSTRUO %d (HP antes=%d)\n", hero->id, m->id, m->hp);
            m->hp -= hero->attack;
            if (m->hp <= 0) printf("[MONSTRUO %d] Muere\n", m->id);
            else printf("[MONSTRUO %d] HP restante: %d\n", m->id, m->hp);
        }
        pthread_mutex_unlock(&cfg->mutex);

        // movimiento
        if (!hero->en_combate) {
            pthread_mutex_lock(&cfg->mutex);
            if (hero->hp > 0) {
                if (hero->path_step < hero->path_len) {
                    hero->posActual = hero->path[hero->path_step++];
                    printf("[HEROE %d] Se mueve a (%d,%d)\n",
                           hero->id, hero->posActual.x, hero->posActual.y);
                } else {
                    hero->path_finished = 1;
                    hero->escapado = 1;
                    printf("[HEROE %d] Llega al final y escapa (HP=%d)\n", hero->id, hero->hp);
                    actualizarEstadoJuego(cfg);
                }
            }
            pthread_mutex_unlock(&cfg->mutex);
        }

        sleep(1);
    }

    pthread_mutex_lock(&cfg->mutex);
    if (hero->hp <= 0) {
        printf("[HEROE %d] Ha muerto.\n", hero->id);
        actualizarEstadoJuego(cfg);
    }
    pthread_mutex_unlock(&cfg->mutex);

    return NULL;
}

// hilo que controla el comportamiento del monstruo
void* hiloMonstruo(void* arg) {
    ArgsMonstruo* a = (ArgsMonstruo*) arg;
    GameConfig* cfg = a->cfg;
    Monster* m = &cfg->monsters[a->id];

    pthread_mutex_lock(&cfg->mutex);
    printf("[MONSTRUO %d] Inicia en (%d,%d), HP=%d, Vision=%d\n",
           m->id, m->pos.x, m->pos.y, m->hp, m->vision);
    pthread_mutex_unlock(&cfg->mutex);

    while (1) {
        sleep(1);

        pthread_mutex_lock(&cfg->mutex);
        if (!cfg->juegoActivo || m->hp <= 0) {
            pthread_mutex_unlock(&cfg->mutex);
            break;
        }
        if (heroesActivosCount(cfg) == 0) {
            cfg->juegoActivo = 0;
            pthread_mutex_unlock(&cfg->mutex);
            break;
        }

        // vision
        if (!m->alertado) {
            int heroe_visto_id = -1, dist_min = 1e9;
            for (int i = 0; i < cfg->hero_count; i++) {
                Hero* h = &cfg->heroes[i];
                if (h->hp <= 0 || h->escapado) continue;
                int d = distanciaManhattan(h->posActual, m->pos);
                if (d <= m->vision && d < dist_min) {
                    dist_min = d;
                    heroe_visto_id = i;
                }
            }
            if (heroe_visto_id != -1) {
                m->alertado = 1;
                m->target_hero_id = heroe_visto_id;
                printf("[MONSTRUO %d] Ve al HEROE %d a distancia %d\n",
                       m->id, heroe_visto_id + 1, dist_min);
                alertarMonstruos(cfg, a->id, heroe_visto_id);
            }
        }

        // ataque o movimiento
        if (m->alertado) {
            if (m->target_hero_id == -1 ||
                cfg->heroes[m->target_hero_id].hp <= 0 ||
                cfg->heroes[m->target_hero_id].escapado) {
                int nuevo = -1, dmin = 1e9;
                for (int i = 0; i < cfg->hero_count; ++i) {
                    Hero* h = &cfg->heroes[i];
                    if (h->hp <= 0 || h->escapado) continue;
                    int d = distanciaManhattan(h->posActual, m->pos);
                    if (d < dmin) { dmin = d; nuevo = i; }
                }
                if (nuevo == -1) { m->alertado = 0; m->target_hero_id = -1; }
                else { m->target_hero_id = nuevo; }
            } else {
                Hero* t = &cfg->heroes[m->target_hero_id];
                int d = distanciaManhattan(t->posActual, m->pos);
                if (d <= m->range) {
                    printf("[MONSTRUO %d] Ataca a HEROE %d (HP antes: %d)\n", m->id, t->id, t->hp);
                    t->hp -= m->attack;
                    if (t->hp <= 0) {
                        printf("[HEROE %d] Muere por MONSTRUO %d\n", t->id, m->id);
                        actualizarEstadoJuego(cfg);
                    } else {
                        printf("[HEROE %d] HP restante: %d\n", t->id, t->hp);
                    }
                } else {
                    if (t->posActual.x > m->pos.x) m->pos.x++;
                    else if (t->posActual.x < m->pos.x) m->pos.x--;
                    else if (t->posActual.y > m->pos.y) m->pos.y++;
                    else if (t->posActual.y < m->pos.y) m->pos.y--;
                    printf("[MONSTRUO %d] Avanza hacia HEROE %d -> (%d,%d)\n",
                           m->id, t->id, m->pos.x, m->pos.y);
                }
            }
        }

        pthread_mutex_unlock(&cfg->mutex);
    }

    pthread_mutex_lock(&cfg->mutex);
    printf("[MONSTRUO %d] Termina hilo en (%d,%d)\n", m->id, m->pos.x, m->pos.y);
    pthread_mutex_unlock(&cfg->mutex);
    return NULL;
}

int main(int argc, char **argv) {
    const char *path = (argc > 1) ? argv[1] : "config.txt";

    GameConfig cfg;
    if (leerConfig(path, &cfg) != 0) return 1;

    if (cfg.hero_count < 1) { printf("No hay heroes configurados.\n"); return 1; }
    if (cfg.monster_count < 0) cfg.monster_count = 0;

    pthread_mutex_init(&cfg.mutex, NULL);
    cfg.juegoActivo = 1;

    pthread_t heroes[cfg.hero_count], monstruos[cfg.monster_count];
    ArgsHeroe args_h[cfg.hero_count];
    ArgsMonstruo args_m[cfg.monster_count];

    for (int i = 0; i < cfg.hero_count; i++) {
        args_h[i].cfg = &cfg;
        args_h[i].id = i;
        pthread_create(&heroes[i], NULL, hiloHeroe, &args_h[i]);
    }

    for (int i = 0; i < cfg.monster_count; i++) {
        args_m[i].cfg = &cfg;
        args_m[i].id = i;
        pthread_create(&monstruos[i], NULL, hiloMonstruo, &args_m[i]);
    }

    for (int i = 0; i < cfg.hero_count; i++) pthread_join(heroes[i], NULL);
    for (int i = 0; i < cfg.monster_count; i++) pthread_join(monstruos[i], NULL);

    pthread_mutex_destroy(&cfg.mutex);
    printf("\n=== Simulacion terminada ===\n");
    return 0;
}
