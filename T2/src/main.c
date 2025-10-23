#include <stdio.h>
#include <pthread.h>
#include <unistd.h> // para el sleep()
#include <stdlib.h> // para abs()
#include "config.h"

// Structs para pasar args a hilos
typedef struct {
    GameConfig* cfg;
    int id;
} ArgsHeroe;

typedef struct {
    GameConfig* cfg;
    int id;
} ArgsMonstruo;


// Funciones utilidad
int distanciaManhattan(Point a, Point b) {
    return abs(a.x - b.x) + abs(a.y - b.y);
}

int todosLosHeroesMuertos(GameConfig* cfg) {
    for (int i = 0; i < cfg->hero_count; i++) {
        if (cfg->heroes[i].hp > 0)
            return 0; // Al menos uno vivo
    }
    return 1; // Todos muertos
}

// Alerta a otros monstruos cercanos
void alertarMonstruos(GameConfig* cfg, int idx_monstruo_alerta, int id_heroe_visto) {
    Monster* m = &cfg->monsters[idx_monstruo_alerta];

    for (int i = 0; i < cfg->monster_count; i++) {
        if (i == idx_monstruo_alerta) continue; 
        
        Monster* otro = &cfg->monsters[i];
        if (otro->hp <= 0 || otro->alertado) continue; 

        int d = distanciaManhattan(m->pos, otro->pos);
        if (d <= m->vision) {
            otro->alertado = 1;
            otro->target_hero_id = id_heroe_visto; // Le pasa el objetivo
            
            printf("[MONSTRUO %d] Alerta a MONSTRUO %d (Vieron al Heroe %d)\n",
                   m->id, otro->id, id_heroe_visto + 1);
        }
    }
}


// --- HILOS ---

void* hiloHeroe(void* arg) {
    ArgsHeroe* a = (ArgsHeroe*) arg;
    GameConfig* cfg = a->cfg;
    Hero* hero = &cfg->heroes[a->id]; 
    
    hero->posActual = hero->start;
    hero->path_step = 0;
    hero->path_finished = 0;
    hero->en_combate = 0;

    printf("[HEROE %d] Iniciando en (%d,%d) con %d HP\n",
           hero->id, hero->posActual.x, hero->posActual.y, hero->hp);

    while (hero->hp > 0 && cfg->juegoActivo && !hero->path_finished) {
        
        hero->en_combate = 0;
        Monster* monstruo_atacante = NULL;

        // --- FASE DE COMBATE ---
        pthread_mutex_lock(&cfg->mutex);
        
        // 1. Buscar monstruo en rango
        for (int i = 0; i < cfg->monster_count; i++) {
            Monster* m = &cfg->monsters[i];
            if (m->hp <= 0) continue;

            int d = distanciaManhattan(hero->posActual, m->pos);
            if (d <= hero->range) {
                hero->en_combate = 1;
                monstruo_atacante = m;
                break; // Ataca a uno por turno
            }
        }

        // 2. Si encuentra, ataca.
        if (hero->en_combate && monstruo_atacante != NULL) {
            Monster* m = monstruo_atacante;
            
            printf("[HEROE %d] Ataca a MONSTRUO %d (HP antes=%d)\n", hero->id, m->id, m->hp);
            m->hp -= hero->attack;
            
            if (m->hp <= 0) {
                printf("[MONSTRUO %d] Muere\n", m->id);
            } else {
                printf("[MONSTRUO %d] HP restante: %d\n", m->id, m->hp);
            }
        }
        
        pthread_mutex_unlock(&cfg->mutex);

        // --- FASE DE MOVIMIENTO ---
        if (!hero->en_combate && hero->hp > 0) {
            if (hero->path_step < hero->path_len) {
                hero->posActual = hero->path[hero->path_step];
                hero->path_step++;
                
                pthread_mutex_lock(&cfg->mutex);
                printf("[HEROE %d] Se mueve a (%d,%d)\n",
                       hero->id, hero->posActual.x, hero->posActual.y);
                pthread_mutex_unlock(&cfg->mutex);

            } else {
                hero->path_finished = 1;
            }
        }

        sleep(1); 
    }

    // --- Fin del Hilo ---
    pthread_mutex_lock(&cfg->mutex);
    if (hero->hp <= 0) {
        printf("[HEROE %d] Ha muerto.\n", hero->id);
    } else if (hero->path_finished) {
        printf("[HEROE %d] Llega al final del camino con vida (HP=%d)\n", hero->id, hero->hp);
    }
    
    if (todosLosHeroesMuertos(cfg)) {
        cfg->juegoActivo = 0;
        printf("=== Todos los heroes han caido. Fin de la simulacion. ===\n");
    }
    pthread_mutex_unlock(&cfg->mutex);

    return NULL;
}


void* hiloMonstruo(void* arg) {
    ArgsMonstruo* a = (ArgsMonstruo*) arg;
    GameConfig* cfg = a->cfg;
    Monster* m = &cfg->monsters[a->id];

    pthread_mutex_lock(&cfg->mutex);
    printf("[MONSTRUO %d] Inicia en (%d,%d), HP=%d, Vision=%d\n",
           m->id, m->pos.x, m->pos.y, m->hp, m->vision);
    pthread_mutex_unlock(&cfg->mutex);

    while (m->hp > 0 && cfg->juegoActivo) {
        sleep(1); 

        if (!cfg->juegoActivo) break; 

        pthread_mutex_lock(&cfg->mutex);
        
        if(m->hp <= 0) {
             pthread_mutex_unlock(&cfg->mutex);
             break;
        }

        // --- FASE DE VISION (PASIVO) ---
        if (!m->alertado) {
            int heroe_visto_id = -1;
            int dist_min = 99999;
            
            // Buscar heroe vivo mas cercano
            for(int i = 0; i < cfg->hero_count; i++) {
                Hero* h = &cfg->heroes[i];
                if (h->hp <= 0) continue;
                
                int d = distanciaManhattan(h->posActual, m->pos);
                if (d <= m->vision && d < dist_min) {
                    dist_min = d;
                    heroe_visto_id = i; 
                }
            }
            
            if (heroe_visto_id != -1) {
                m->alertado = 1;
                m->target_hero_id = heroe_visto_id;
                
                printf("[MONSTRUO %d] Ve al HEROE %d a distancia %d! Se alerta!\n",
                       m->id, heroe_visto_id + 1, dist_min);
                
                alertarMonstruos(cfg, a->id, heroe_visto_id);
            }
        }

        // --- FASE DE ACCION (ALERTA) ---
        if (m->alertado) {
            // Validar objetivo
            if (m->target_hero_id == -1 || cfg->heroes[m->target_hero_id].hp <= 0) {
                // Si murio, buscar otro
                m->alertado = 0; 
                m->target_hero_id = -1;
            } else {
                
                Hero* target = &cfg->heroes[m->target_hero_id];
                int d = distanciaManhattan(target->posActual, m->pos);

                if (d <= m->range) {
                    // --- ATACAR ---
                    printf("[MONSTRUO %d] Ataca a HEROE %d! (HP antes: %d)\n",
                           m->id, target->id, target->hp);
                    target->hp -= m->attack;

                    if (target->hp <= 0) {
                        printf("[HEROE %d] Muere bajo el ataque del MONSTRUO %d.\n",
                               target->id, m->id);
                        
                        if (todosLosHeroesMuertos(cfg)) {
                            cfg->juegoActivo = 0;
                            printf("=== El MONSTRUO %d ha matado al ultimo heroe! ===\n", m->id);
                        }
                    } else {
                        printf("[HEROE %d] HP restante: %d\n", target->id, target->hp);
                    }

                } else {
                    // --- MOVERSE ---
                    if (target->posActual.x > m->pos.x) m->pos.x++;
                    else if (target->posActual.x < m->pos.x) m->pos.x--;
                    else if (target->posActual.y > m->pos.y) m->pos.y++;
                    else if (target->posActual.y < m->pos.y) m->pos.y--;
                    
                    printf("[MONSTRUO %d] Avanza hacia HEROE %d -> (%d,%d)\n",
                           m->id, target->id, m->pos.x, m->pos.y);
                }
            }
        }
        
        pthread_mutex_unlock(&cfg->mutex);
    }

    printf("[MONSTRUO %d] Finaliza hilo en (%d,%d)\n", m->id, m->pos.x, m->pos.y);
    return NULL;
}


// --- MAIN ---

int main() {
    GameConfig cfg;
    if (leerConfig("config.txt", &cfg) != 0) {
        return 1;
    }

    pthread_mutex_init(&cfg.mutex, NULL);
    cfg.juegoActivo = 1;

    pthread_t heroes[cfg.hero_count];
    pthread_t monstruos[cfg.monster_count];
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

    // Esperar que todos terminen
    for (int i = 0; i < cfg.hero_count; i++) {
        pthread_join(heroes[i], NULL);
    }
    for (int i = 0; i < cfg.monster_count; i++) {
        pthread_join(monstruos[i], NULL);
    }

    pthread_mutex_destroy(&cfg.mutex);

    printf("\n=== Simulacion terminada ===\n");
    return 0;
}