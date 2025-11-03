#include "config.h"
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static void init_cfg(GameConfig *cfg) {
    cfg->width = 0;
    cfg->height = 0;
    cfg->monster_count = 0;
    cfg->hero_count = 0;

    for (int i = 0; i < MAX_HEROES; ++i) {
        Hero *h = &cfg->heroes[i];
        h->id = i + 1;
        h->hp = 0;
        h->attack = 0;
        h->range = 0;
        h->start.x = h->start.y = 0;
        h->posActual = h->start;
        h->path_len = 0;
        h->path_step = 0;
        h->en_combate = 0;
        h->path_finished = 0;
        h->escapado = 0;
    }
    for (int i = 0; i < MAX_MONSTERS; ++i) {
        Monster *m = &cfg->monsters[i];
        m->id = i + 1;
        m->hp = 0;
        m->attack = 0;
        m->vision = 0;
        m->range = 0;
        m->pos.x = m->pos.y = 0;
        m->alertado = 0;
        m->target_hero_id = -1;
    }
}

// Lee coordenadas tipo "(x,y) (x,y) ..." hasta fin de línea.
static void parse_path_until_eol(FILE *f, Point *arr, int *len) {
    int x, y; char c; *len = 0;

    int ch = fgetc(f);
    while (ch == ' ') ch = fgetc(f);
    if (ch != EOF) ungetc(ch, f);

    while (*len < MAX_PATH && fscanf(f, " (%d,%d)", &x, &y) == 2) {
        arr[*len].x = x; arr[*len].y = y; (*len)++;
        c = (char)fgetc(f);
        if (c == '\n' || c == '\r' || c == EOF) break;
        ungetc(c, f);
    }
    while ((c = (char)fgetc(f)) != '\n' && c != EOF) {}
}

static void norm_key(const char *src, char *dst, size_t n) {
    size_t j = 0;
    for (size_t i = 0; src[i] && j + 1 < n; ++i)
        if (src[i] != '_') dst[j++] = src[i];
    dst[j] = '\0';
}

int leerConfig(const char *nombreArchivo, GameConfig *cfg) {
    FILE *f = fopen(nombreArchivo, "r");
    if (f == NULL) {
        printf("No se pudo abrir el archivo de configuracion: %s\n", nombreArchivo);
        return 1;
    }

    init_cfg(cfg);

    char word[128];
    while (fscanf(f, "%127s", word) == 1) {
        if (word[0] == '#') { int c; while ((c = fgetc(f)) != '\n' && c != EOF) {} continue; }

        if (strcmp(word, "GRID_SIZE") == 0) { fscanf(f, "%d %d", &cfg->width, &cfg->height); continue; }
        if (strcmp(word, "HERO_COUNT") == 0) {
            int hc; fscanf(f, "%d", &hc);
            if (hc > MAX_HEROES) { printf("[ADVERTENCIA] HERO_COUNT=%d > MAX_HEROES=%d. Truncado.\n", hc, MAX_HEROES); hc = MAX_HEROES; }
            if (hc < 1) { printf("[ADVERTENCIA] HERO_COUNT<1. Se ajusta a 1.\n"); hc = 1; }
            cfg->hero_count = hc; continue;
        }
        if (strcmp(word, "MONSTER_COUNT") == 0) {
            int mc; fscanf(f, "%d", &mc);
            if (mc > MAX_MONSTERS) { printf("[ADVERTENCIA] MONSTER_COUNT=%d > MAX_MONSTERS=%d. Truncado.\n", mc, MAX_MONSTERS); mc = MAX_MONSTERS; }
            if (mc < 0) mc = 0;
            cfg->monster_count = mc; continue;
        }

        // ----- Formato extendido N héroes -----
        if (strcmp(word, "HERO") == 0) {
            int id; char campo[64]; if (fscanf(f, "%d %63s", &id, campo) != 2) continue;
            if (id < 1 || id > MAX_HEROES) { printf("[ADVERTENCIA] HERO id=%d fuera de rango. Ignorado.\n", id); int c; while ((c = fgetc(f)) != '\n' && c != EOF) {} continue; }
            if (id > cfg->hero_count) cfg->hero_count = id;
            Hero *h = &cfg->heroes[id-1];

            if      (strcmp(campo, "HP") == 0) fscanf(f, "%d", &h->hp);
            else if (strcmp(campo, "ATTACK_DAMAGE") == 0) fscanf(f, "%d", &h->attack);
            else if (strcmp(campo, "ATTACK_RANGE") == 0) fscanf(f, "%d", &h->range);
            else if (strcmp(campo, "START") == 0) { fscanf(f, "%d %d", &h->start.x, &h->start.y); h->posActual = h->start; }
            else if (strcmp(campo, "PATH") == 0) { parse_path_until_eol(f, h->path, &h->path_len); }
            else { int c; while ((c = fgetc(f)) != '\n' && c != EOF) {} }
            continue;
        }

        if (strcmp(word, "MONSTER") == 0) {
            int id; char campo[64]; if (fscanf(f, "%d %63s", &id, campo) != 2) continue;
            if (id < 1 || id > MAX_MONSTERS) { printf("[ADVERTENCIA] MONSTER id=%d fuera de rango. Ignorado.\n", id); int c; while ((c = fgetc(f)) != '\n' && c != EOF) {} continue; }
            if (id > cfg->monster_count) cfg->monster_count = id;
            Monster *m = &cfg->monsters[id-1];
            m->target_hero_id = -1;

            if      (strcmp(campo, "HP") == 0) fscanf(f, "%d", &m->hp);
            else if (strcmp(campo, "ATTACK_DAMAGE") == 0) fscanf(f, "%d", &m->attack);
            else if (strcmp(campo, "VISION_RANGE") == 0) fscanf(f, "%d", &m->vision);
            else if (strcmp(campo, "ATTACK_RANGE") == 0) fscanf(f, "%d", &m->range);
            else if (strcmp(campo, "COORDS") == 0) fscanf(f, "%d %d", &m->pos.x, &m->pos.y);
            else { int c; while ((c = fgetc(f)) != '\n' && c != EOF) {} }
            continue;
        }

        // ----- Formato base 1 héroe -----
        char key[128]; norm_key(word, key, sizeof(key));

        if (strncmp(key, "HERO", 4) == 0) {
            Hero *h = &cfg->heroes[0]; h->id = 1; if (cfg->hero_count < 1) cfg->hero_count = 1;
            if      (strcmp(key, "HEROHP") == 0) fscanf(f, "%d", &h->hp);
            else if (strcmp(key, "HEROATTACKDAMAGE") == 0) fscanf(f, "%d", &h->attack);
            else if (strcmp(key, "HEROATTACKRANGE") == 0) fscanf(f, "%d", &h->range);
            else if (strcmp(key, "HEROSTART") == 0) { fscanf(f, "%d %d", &h->start.x, &h->start.y); h->posActual = h->start; }
            else if (strcmp(key, "HEROPATH") == 0) { parse_path_until_eol(f, h->path, &h->path_len); }
            else { int c; while ((c = fgetc(f)) != '\n' && c != EOF) {} }
            continue;
        }

        if (strncmp(key, "MONSTER", 7) == 0) {
            const char *p = word + 7; if (*p == '_') ++p; int id = 0;
            while (*p && isdigit((unsigned char)*p)) { id = id * 10 + (*p - '0'); ++p; }
            if (id < 1 || id > MAX_MONSTERS) { printf("[ADVERTENCIA] MONSTER_%d fuera de rango. Ignorado.\n", id); int c; while ((c = fgetc(f)) != '\n' && c != EOF) {} continue; }
            if (id > cfg->monster_count) cfg->monster_count = id;
            Monster *m = &cfg->monsters[id-1];

            if      (strstr(key, "HP")) fscanf(f, "%d", &m->hp);
            else if (strstr(key, "ATTACKDAMAGE")) fscanf(f, "%d", &m->attack);
            else if (strstr(key, "VISIONRANGE")) fscanf(f, "%d", &m->vision);
            else if (strstr(key, "ATTACKRANGE")) fscanf(f, "%d", &m->range);
            else if (strstr(key, "COORDS")) fscanf(f, "%d %d", &m->pos.x, &m->pos.y);
            else { int c; while ((c = fgetc(f)) != '\n' && c != EOF) {} }
            continue;
        }

        int c; while ((c = fgetc(f)) != '\n' && c != EOF) {}
    }

    if (cfg->hero_count < 1) cfg->hero_count = 1;

    fclose(f);
    return 0;
}
