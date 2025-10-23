#include "config.h"
#include <stdio.h>
#include <string.h>

int leerConfig(const char *nombreArchivo, GameConfig *cfg) {
    FILE *f = fopen(nombreArchivo, "r");
    if (f == NULL) {
        printf("No se pudo abrir el archivo de configuracion.\n");
        return 1;
    }

    cfg->width = 0;
    cfg->height = 0;
    cfg->monster_count = 0;
    cfg->hero_count = 0;

    char palabra[64];
    while (fscanf(f, "%63s", palabra) == 1) { 
                                                        
        if (strcmp(palabra, "GRID_SIZE") == 0) {
            fscanf(f, "%d %d", &cfg->width, &cfg->height);
        }
        // --- CONTEOS ---
        else if (strcmp(palabra, "HERO_COUNT") == 0) {
            fscanf(f, "%d", &cfg->hero_count);
        }
        else if (strcmp(palabra, "MONSTER_COUNT") == 0) {
            fscanf(f, "%d", &cfg->monster_count);
        }
        
        // --- PARSEO HEROE ---
        else if (strcmp(palabra, "HERO") == 0) {
            int id;
            char campo[64];
            fscanf(f, "%d %s", &id, campo); 
            
            if (id <= 0 || id > MAX_HEROES) continue;
            Hero *h = &cfg->heroes[id - 1];
            h->id = id;
            h->path_step = 0;
            h->en_combate = 0;
            h->path_finished = 0;

            if (strcmp(campo, "HP") == 0)
                fscanf(f, "%d", &h->hp);
            else if (strcmp(campo, "ATTACK_DAMAGE") == 0)
                fscanf(f, "%d", &h->attack);
            else if (strcmp(campo, "ATTACK_RANGE") == 0)
                fscanf(f, "%d", &h->range);
            else if (strcmp(campo, "START") == 0)
                fscanf(f, "%d %d", &h->start.x, &h->start.y);
            else if (strcmp(campo, "PATH") == 0) {
                h->path_len = 0;
                int x, y;
                char c;
                while (fscanf(f, " (%d,%d)", &x, &y) == 2) {
                    h->path[h->path_len].x = x;
                    h->path[h->path_len].y = y;
                    h->path_len++;
                    if (h->path_len >= MAX_PATH) break;
                    
                    c = fgetc(f); 
                    if (c == '\n' || c == EOF) break;
                }
            }
        }
        
        // --- PARSEO MONSTRUO ---
        else if (strcmp(palabra, "MONSTER") == 0) {
            int id;
            char campo[64];
            fscanf(f, "%d %s", &id, campo); 

            if (id <= 0 || id > MAX_MONSTERS) continue;
            Monster *m = &cfg->monsters[id - 1];
            m->id = id;
            m->target_hero_id = -1; 
            m->alertado = 0;

            if (strcmp(campo, "HP") == 0)
                fscanf(f, "%d", &m->hp);
            else if (strcmp(campo, "ATTACK_DAMAGE") == 0)
                fscanf(f, "%d", &m->attack);
            else if (strcmp(campo, "VISION_RANGE") == 0)
                fscanf(f, "%d", &m->vision);
            else if (strcmp(campo, "ATTACK_RANGE") == 0)
                fscanf(f, "%d", &m->range);
            else if (strcmp(campo, "COORDS") == 0)
                fscanf(f, "%d %d", &m->pos.x, &m->pos.y);
        }
    }

    fclose(f);
    return 0;
}