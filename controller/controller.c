/*
 * ============================================================
 *  CONTROLLER — Loop principal + Auto-Play (IA do Pac-Man)
 * ============================================================
 *  Auto-Play usa BFS no grafo do labirinto para encontrar
 *  o pellet mais proximo nao coletado, desviando de fantasmas.
 *  O Pac-Man escolhe o proximo passo pelo caminho BFS.
 * ============================================================
 */

#include "pacman.h"
#include <math.h>
#ifdef _WIN32
#include <windows.h>
#include <mmsystem.h>
#endif

static int controller_play_audio_once(const char *base_name, const char *alias) {
#ifdef _WIN32
    static const char *extensions[] = {".wav", ".mp3", ".wma", ".ogg"};
    for (int i = 0; i < 4; i++) {
        char pattern[1024];
        snprintf(pattern, sizeof(pattern), "audio\\%s%s", base_name, extensions[i]);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            FindClose(h);

            char close_cmd[64];
            snprintf(close_cmd, sizeof(close_cmd), "close %s", alias);
            mciSendStringA(close_cmd, NULL, 0, NULL);

            const char *media_type = strstr(pattern, ".wav") ? "waveaudio" : "mpegvideo";
            char open_cmd[1024];
            snprintf(open_cmd, sizeof(open_cmd), "open \"%s\" type %s alias %s", pattern, media_type, alias);
            if (mciSendStringA(open_cmd, NULL, 0, NULL) == 0) {
                char play_cmd[64];
                snprintf(play_cmd, sizeof(play_cmd), "play %s", alias);
                mciSendStringA(play_cmd, NULL, 0, NULL);
                return 1;
            }
        }
    }
#endif
    return 0;
}

static int controller_play_audio_and_wait(const char *base_name, const char *alias) {
#ifdef _WIN32
    static const char *extensions[] = {".wav", ".mp3", ".wma", ".ogg"};
    for (int i = 0; i < 4; i++) {
        char pattern[1024];
        snprintf(pattern, sizeof(pattern), "audio\\%s%s", base_name, extensions[i]);
        WIN32_FIND_DATAA fd;
        HANDLE h = FindFirstFileA(pattern, &fd);
        if (h != INVALID_HANDLE_VALUE) {
            FindClose(h);

            char close_cmd[64];
            snprintf(close_cmd, sizeof(close_cmd), "close %s", alias);
            mciSendStringA(close_cmd, NULL, 0, NULL);

            const char *media_type = strstr(pattern, ".wav") ? "waveaudio" : "mpegvideo";
            char open_cmd[1024];
            snprintf(open_cmd, sizeof(open_cmd), "open \"%s\" type %s alias %s", pattern, media_type, alias);
            if (mciSendStringA(open_cmd, NULL, 0, NULL) == 0) {
                char play_cmd[64];
                snprintf(play_cmd, sizeof(play_cmd), "play %s wait", alias);
                mciSendStringA(play_cmd, NULL, 0, NULL);
                return 1;
            }
        }
    }
#endif
    return 0;
}

static void controller_stop_audio_alias(const char *alias) {
#ifdef _WIN32
    char close_cmd[64];
    snprintf(close_cmd, sizeof(close_cmd), "close %s", alias);
    mciSendStringA(close_cmd, NULL, 0, NULL);
#endif
}

/* ======================== INPUT ======================== */

void controller_handle_input(GameModel *m) {
    while (kbhit_custom()) {
        int ch = getch_custom();
#ifdef _WIN32
        if (ch == 0 || ch == 224) {
            int arrow = getch_custom();
            if (arrow == 72) { m->desired_dx = 0;  m->desired_dy = -1; }
            if (arrow == 80) { m->desired_dx = 0;  m->desired_dy = 1;  }
            if (arrow == 75) { m->desired_dx = -1; m->desired_dy = 0;  }
            if (arrow == 77) { m->desired_dx = 1;  m->desired_dy = 0;  }
            continue;
        }
#else
        if (ch == 27 && kbhit_custom()) {
            int c2 = getch_custom();
            if (c2 == '[' && kbhit_custom()) {
                int c3 = getch_custom();
                if (c3 == 'A') { m->desired_dx = 0;  m->desired_dy = -1; }
                if (c3 == 'B') { m->desired_dx = 0;  m->desired_dy = 1;  }
                if (c3 == 'D') { m->desired_dx = -1; m->desired_dy = 0;  }
                if (c3 == 'C') { m->desired_dx = 1;  m->desired_dy = 0;  }
            }
            continue;
        }
#endif
        if (ch == 'w' || ch == 'W') { m->desired_dx = 0;  m->desired_dy = -1; }
        if (ch == 's' || ch == 'S') { m->desired_dx = 0;  m->desired_dy = 1;  }
        if (ch == 'a' || ch == 'A') { m->desired_dx = -1; m->desired_dy = 0;  }
        if (ch == 'd' || ch == 'D') { m->desired_dx = 1;  m->desired_dy = 0;  }
        if (ch == 'q' || ch == 'Q') { m->running = 0; m->state = STATE_QUIT; }
        if (ch == 'p' || ch == 'P') {
            if (m->state == STATE_PLAYING) m->state = STATE_PAUSED;
            else if (m->state == STATE_PAUSED) m->state = STATE_PLAYING;
        }
    }
}

void controller_menu_input(GameModel *m) {
    enum { MENU_PLAY = 0, MENU_AUTO = 1, MENU_HELP = 2, MENU_SCORES = 3, MENU_QUIT = 4, MENU_COUNT = 5 };
    while (kbhit_custom()) {
        int ch = getch_custom();
#ifdef _WIN32
        if (ch == 0 || ch == 224) {
            int arrow = getch_custom();
            if (arrow == 72) { /* up */
                m->menu_selected = (m->menu_selected - 1 + MENU_COUNT) % MENU_COUNT;
            } else if (arrow == 80) { /* down */
                m->menu_selected = (m->menu_selected + 1) % MENU_COUNT;
            }
            continue;
        }
#else
        if (ch == 27 && kbhit_custom()) {
            int c2 = getch_custom();
            if (c2 == '[' && kbhit_custom()) {
                int c3 = getch_custom();
                if (c3 == 'A') m->menu_selected = (m->menu_selected - 1 + MENU_COUNT) % MENU_COUNT; /* up */
                if (c3 == 'B') m->menu_selected = (m->menu_selected + 1) % MENU_COUNT;             /* down */
            }
            continue;
        }
#endif

        /* Atalhos de navegação */
        if (ch == 'w' || ch == 'W') { m->menu_selected = (m->menu_selected - 1 + MENU_COUNT) % MENU_COUNT; continue; }
        if (ch == 's' || ch == 'S') { m->menu_selected = (m->menu_selected + 1) % MENU_COUNT; continue; }

        /* Atalhos diretos */
        if (ch == 'a' || ch == 'A') {
            m->auto_play = 1;
            m->auto_pellets_eaten = 0;
            m->auto_ghosts_eaten = 0;
            m->auto_update_counter = 0;
            strncpy(m->auto_status, "BFS", sizeof(m->auto_status) - 1);
            m->state = STATE_SKIN_SELECT;
            return;
        }
        if (ch == 'h' || ch == 'H') {
            m->state = STATE_SCORES;
            return;
        }
        if (ch == 'e' || ch == 'E') {
            m->state = STATE_HELP;
            return;
        }
        if (ch == 'q' || ch == 'Q') {
            m->running = 0;
            m->state = STATE_QUIT;
            return;
        }

        /* ENTER / SPACE: confirma seleção atual */
        if (ch == '\r' || ch == '\n' || ch == ' ') {
            switch (m->menu_selected) {
                case MENU_PLAY:
                    m->auto_play = 0;
                    m->state = STATE_SKIN_SELECT;
                    return;
                case MENU_AUTO:
                    m->auto_play = 1;
                    m->auto_pellets_eaten = 0;
                    m->auto_ghosts_eaten = 0;
                    m->auto_update_counter = 0;
                    strncpy(m->auto_status, "BFS", sizeof(m->auto_status) - 1);
                    m->state = STATE_SKIN_SELECT;
                    return;
                case MENU_HELP:
                    m->state = STATE_HELP;
                    return;
                case MENU_SCORES:
                    m->state = STATE_SCORES;
                    return;
                case MENU_QUIT:
                default:
                    m->running = 0;
                    m->state = STATE_QUIT;
                    return;
            }
        }
    }
}

/* ======================== AUTO-PLAY ======================== */
/*
 * Algoritmo:
 * 1. Executa BFS a partir da posicao do Pac-Man no grafo do labirinto.
 * 2. Encontra o pellet/power-up mais proximo (menor dist BFS).
 *    Se power estiver ativo, prioriza fantasmas vulneraveis.
 * 3. Reconstroi o caminho e extrai o primeiro passo.
 * 4. Aplica penalidade nos vertices proximos a fantasmas ativos
 *    para evitar colisoes.
 */

/* ======================== META-HEURISTICA (SIMULATED ANNEALING) ======================== */

typedef struct { int dx, dy; } Move;

static const Move MOVES4[4] = {{1,0},{-1,0},{0,1},{0,-1}};

static int is_opposite(int dx1, int dy1, int dx2, int dy2) { return dx1 == -dx2 && dy1 == -dy2; }

static int wrap_x(int x) {
    if (x < 0) return MAP_W - 1;
    if (x >= MAP_W) return 0;
    return x;
}

static int in_bounds_y(int y) { return y >= 0 && y < MAP_H; }

static int valid_moves_from(GameModel *m, int x, int y, int cur_dx, int cur_dy, Move *out, int max_out) {
    int n = 0;
    for (int i = 0; i < 4; i++) {
        int dx = MOVES4[i].dx, dy = MOVES4[i].dy;
        if (cur_dx || cur_dy) {
            /* Evita ficar "chacoalhando" voltando no mesmo frame, mas permite se for a unica saida */
            if (is_opposite(dx, dy, cur_dx, cur_dy)) continue;
        }
        int nx = wrap_x(x + dx);
        int ny = y + dy;
        if (!in_bounds_y(ny)) continue;
        if (!model_is_walkable(m, nx, ny, 0)) continue;
        if (n < max_out) out[n] = (Move){dx, dy};
        n++;
    }
    /* Se nao tinha nenhuma (ex.: corredor), libera reverso */
    if (n == 0) {
        for (int i = 0; i < 4; i++) {
            int dx = MOVES4[i].dx, dy = MOVES4[i].dy;
            int nx = wrap_x(x + dx);
            int ny = y + dy;
            if (!in_bounds_y(ny)) continue;
            if (!model_is_walkable(m, nx, ny, 0)) continue;
            if (n < max_out) out[n] = (Move){dx, dy};
            n++;
        }
    }
    return n;
}

static double ghost_penalty(GameModel *m, int x, int y) {
    double p = 0.0;
    for (int i = 0; i < GHOSTS; i++) {
        Ghost *g = &m->ghosts[i];
        if (g->in_house) continue;
        int d = manhattan(x, y, g->e.x, g->e.y);
        if (!g->vulnerable) {
            if (d == 0) return 1e9;     /* colisao */
            if (d <= 1) p += 800.0;
            else if (d == 2) p += 220.0;
            else if (d == 3) p += 90.0;
            else p += 30.0 / (double)d;
        } else {
            /* Incentiva aproximar quando vulneravel — bem mais forte durante POWER */
            double k = (m->power_timer > 0) ? 120.0 : 18.0;
            if (d == 0) p -= 1500.0; /* "comeu" fantasma: recompensa alta */
            else p -= k / (double)d;
        }
    }
    return p;
}

static double evaluate_path(GameModel *m, const Move *path, int len) {
    int x = m->pacman.x, y = m->pacman.y;
    int cur_dx = m->current_dx, cur_dy = m->current_dy;

    /* Marca rapidamente pellets coletados no rollout (horizonte pequeno) */
    unsigned char eaten[MAP_W * MAP_H];
    memset(eaten, 0, sizeof(eaten));

    double score = 0.0;
    for (int t = 0; t < len; t++) {
        int dx = path[t].dx, dy = path[t].dy;

        /* Se o passo for invalido, penaliza pesado e termina */
        int nx = wrap_x(x + dx);
        int ny = y + dy;
        if (!in_bounds_y(ny) || !model_is_walkable(m, nx, ny, 0)) {
            score -= 2000.0;
            break;
        }

        /* Avanca */
        x = nx; y = ny;
        cur_dx = dx; cur_dy = dy;

        /* Recompensa por coletar */
        int idx = y * MAP_W + x;
        int tile = m->grid[y][x];
        if (!eaten[idx]) {
            if (tile == TILE_PELLET) score += 12.0;
            else if (tile == TILE_POWER) score += 80.0;
            eaten[idx] = 1;
        }

        /*
         * MODO CAÇA: durante o POWER, prioriza encostar em fantasmas vulneráveis.
         * Isso ajuda o auto-play a realmente "matar" os fantasmas em vez de só fugir.
         */
        if (m->power_timer > 0) {
            int best_d = 9999;
            for (int i = 0; i < GHOSTS; i++) {
                Ghost *g = &m->ghosts[i];
                if (g->in_house) continue;
                if (!g->vulnerable) continue;
                int d = manhattan(x, y, g->e.x, g->e.y);
                if (d < best_d) best_d = d;
                if (d == 0) score += 2000.0; /* pegou um fantasma */
            }
            if (best_d < 9999) score += 260.0 / (double)(best_d + 1);
        }

        /* Penaliza risco de fantasma */
        score -= ghost_penalty(m, x, y);

        /* Leve incentivo por manter direcao (movimento mais "fluido") */
        if (t > 0 && path[t].dx == path[t - 1].dx && path[t].dy == path[t - 1].dy) score += 0.8;

        /* Evita becos: menos opcoes futuras = penalidade */
        Move tmp[4];
        int nm = valid_moves_from(m, x, y, cur_dx, cur_dy, tmp, 4);
        if (nm <= 1) score -= 18.0;
    }
    return score;
}

static void rollout_greedy(GameModel *m, int start_x, int start_y, int start_dx, int start_dy, Move *out, int len) {
    int x = start_x, y = start_y;
    int cur_dx = start_dx, cur_dy = start_dy;

    for (int t = 0; t < len; t++) {
        Move cand[4];
        int n = valid_moves_from(m, x, y, cur_dx, cur_dy, cand, 4);

        /* Escolhe melhor passo imediato (1-step lookahead) */
        double best = -1e18;
        Move bestm = (Move){ (cur_dx || cur_dy) ? cur_dx : 1, (cur_dx || cur_dy) ? cur_dy : 0 };
        for (int i = 0; i < n; i++) {
            int nx = wrap_x(x + cand[i].dx);
            int ny = y + cand[i].dy;
            double s = 0.0;
            int tile = m->grid[ny][nx];
            if (tile == TILE_PELLET) s += 12.0;
            if (tile == TILE_POWER)  s += 80.0;
            s -= ghost_penalty(m, nx, ny);
            if (s > best) { best = s; bestm = cand[i]; }
        }
        out[t] = bestm;
        x = wrap_x(x + bestm.dx);
        y = y + bestm.dy;
        cur_dx = bestm.dx; cur_dy = bestm.dy;
        if (!in_bounds_y(y)) break;
    }
}

static void controller_auto_play_sa(GameModel *m) {
    /* Parâmetros: horizonte curto + iterações limitadas p/ rodar em tempo real */
    enum { H = 14, ITERS = 220 };
    const double T0 = 120.0;
    const double COOL = 0.985;

    Move cur[H], best[H], neigh[H];

    rollout_greedy(m, m->pacman.x, m->pacman.y, m->current_dx, m->current_dy, cur, H);
    memcpy(best, cur, sizeof(cur));

    double cur_score = evaluate_path(m, cur, H);
    double best_score = cur_score;

    double T = T0;
    for (int it = 0; it < ITERS; it++) {
        memcpy(neigh, cur, sizeof(cur));

        /* Mutação: escolhe um passo e tenta trocar por outra direção válida */
        int k = rand() % H;

        /* Re-simula até o passo k para saber estado e movimentos válidos */
        int x = m->pacman.x, y = m->pacman.y;
        int pdx = m->current_dx, pdy = m->current_dy;
        for (int t = 0; t < k; t++) {
            x = wrap_x(x + neigh[t].dx);
            y = y + neigh[t].dy;
            pdx = neigh[t].dx; pdy = neigh[t].dy;
            if (!in_bounds_y(y)) break;
        }

        Move cand[4];
        int n = valid_moves_from(m, x, y, pdx, pdy, cand, 4);
        if (n > 0) {
            /* Escolhe um candidato diferente do atual, quando possível */
            int pick = rand() % n;
            for (int tries = 0; tries < n; tries++) {
                Move mm = cand[pick];
                if (mm.dx != neigh[k].dx || mm.dy != neigh[k].dy) { neigh[k] = mm; break; }
                pick = (pick + 1) % n;
            }

            /* Completa o resto com greedy (para manter coerência) */
            int sx = wrap_x(x + neigh[k].dx);
            int sy = y + neigh[k].dy;
            rollout_greedy(m, sx, sy, neigh[k].dx, neigh[k].dy, &neigh[k + 1], H - (k + 1));
        }

        double nscore = evaluate_path(m, neigh, H);
        double delta = nscore - cur_score;

        int accept = 0;
        if (delta >= 0) accept = 1;
        else {
            double r = (double)rand() / (double)RAND_MAX;
            double prob = exp(delta / (T + 1e-9));
            if (r < prob) accept = 1;
        }

        if (accept) {
            memcpy(cur, neigh, sizeof(cur));
            cur_score = nscore;
            if (cur_score > best_score) {
                memcpy(best, cur, sizeof(cur));
                best_score = cur_score;
            }
        }

        T *= COOL;
        if (T < 1.0) T = 1.0;
    }

    /* Próxima ação = primeiro passo do melhor caminho */
    m->desired_dx = best[0].dx;
    m->desired_dy = best[0].dy;
    strncpy(m->auto_status, (m->power_timer > 0) ? "HUNT" : "SA", sizeof(m->auto_status) - 1);
}

static void controller_skin_input(GameModel *m);
static void controller_name_input(GameModel *m);

void controller_auto_play(GameModel *m) {
    if (!m->maze_graph) return;

    /* Meta-heurística: simulated annealing (mais "inteligente" que BFS puro) */
    controller_auto_play_sa(m);
}

/* ======================== UPDATE ======================== */

void controller_update(GameModel *m) {
    if (m->state != STATE_PLAYING) return;
    if (m->phase_start_ticks == 0) m->phase_start_ticks = (int)(clock() / CLOCKS_PER_SEC);

    /* Fase "READY!" */
    if (m->ready_timer > 0) {
        if (m->ready_timer == 1) {
            view_clear_ready(m);
            view_flush();
        }
        m->ready_timer--;
        return;
    }

    view_erase_entities(m);

    /* Controle do Pac-Man: auto ou manual */
    if (m->auto_play) {
        m->auto_update_counter++;
        /* Recalcula rota a cada 3 frames para performance */
        if (m->auto_update_counter >= 3) {
            m->auto_update_counter = 0;
            controller_auto_play(m);
        }
    }

    int prev_score = m->score;
    model_move_pacman(m);

    /* Detecta pellet comido pelo auto-play */
    if (m->auto_play && m->score > prev_score) {
        m->auto_pellets_eaten++;
    }

    int col = model_check_collisions(m);
    if (m->state == STATE_DYING || m->state == STATE_GAMEOVER || m->state == STATE_NAME_ENTRY) return;

    if (col > 0) {
        if (m->auto_play) m->auto_ghosts_eaten++;
        /* Overlay (sem flush/sleep no meio do frame para evitar flicker) */
        if (m->bonus_timer > 0) {
            view_restore_bonus_cell(m, m->bonus_x, m->bonus_y);
        }
        m->bonus_points = col;
        m->bonus_timer  = (m->frame_ms > 0) ? (800 / m->frame_ms) : 12;
        m->bonus_x      = m->pacman.x;
        m->bonus_y      = m->pacman.y;
    }

    model_move_ghosts(m);
    col = model_check_collisions(m);
    if (m->state == STATE_DYING || m->state == STATE_GAMEOVER || m->state == STATE_NAME_ENTRY) return;

    /* Decrementa power timer */
    if (m->power_timer > 0) {
        m->power_timer--;
        if (m->power_timer == 0) {
            for (int i = 0; i < GHOSTS; i++) m->ghosts[i].vulnerable = 0;
            m->ghost_eat_combo = 0;
        }
    }

    /* Atualiza pellets piscando */
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            if (m->grid[y][x] == TILE_POWER)
                view_draw_cell(m, x, y);

    view_draw_entities(m);
    view_draw_hud(m);
    view_draw_bonus(m);
    view_flush();

    if (m->bonus_timer > 0) {
        m->bonus_timer--;
        if (m->bonus_timer == 0) {
            view_restore_bonus_cell(m, m->bonus_x, m->bonus_y);
        }
    }

    /* Vitoria */
    if (m->pellets_left <= 0) {
        m->state = STATE_WIN;

        /* Demonstra todos os 7 algoritmos de sorting */
        if (m->score_history_count > 1) {
            int *tmp = (int *)malloc(m->score_history_count * sizeof(int));

            memcpy(tmp, m->score_history, m->score_history_count * sizeof(int));
            clock_t start = clock();
            bubble_sort(tmp, m->score_history_count);
            m->sort_times[0] = (clock() - start) * 1000000LL / CLOCKS_PER_SEC;

            memcpy(tmp, m->score_history, m->score_history_count * sizeof(int));
            start = clock();
            selection_sort(tmp, m->score_history_count);
            m->sort_times[1] = (clock() - start) * 1000000LL / CLOCKS_PER_SEC;

            memcpy(tmp, m->score_history, m->score_history_count * sizeof(int));
            start = clock();
            insertion_sort(tmp, m->score_history_count);
            m->sort_times[2] = (clock() - start) * 1000000LL / CLOCKS_PER_SEC;

            memcpy(tmp, m->score_history, m->score_history_count * sizeof(int));
            start = clock();
            shell_sort(tmp, m->score_history_count);
            m->sort_times[3] = (clock() - start) * 1000000LL / CLOCKS_PER_SEC;

            memcpy(tmp, m->score_history, m->score_history_count * sizeof(int));
            start = clock();
            merge_sort(tmp, m->score_history_count);
            m->sort_times[4] = (clock() - start) * 1000000LL / CLOCKS_PER_SEC;

            memcpy(tmp, m->score_history, m->score_history_count * sizeof(int));
            start = clock();
            quick_sort(tmp, 0, m->score_history_count - 1);
            m->sort_times[5] = (clock() - start) * 1000000LL / CLOCKS_PER_SEC;

            memcpy(tmp, m->score_history, m->score_history_count * sizeof(int));
            start = clock();
            heap_sort(tmp, m->score_history_count);
            m->sort_times[6] = (clock() - start) * 1000000LL / CLOCKS_PER_SEC;

            free(tmp);
        }
    }

    m->frame_count++;
}

/* ======================== LOOP PRINCIPAL ======================== */

void controller_run(GameModel *m) {
    /* Splash arcade na primeira execução */
    if (!m->splash_shown) {
        m->splash_shown = 1;
        int saved_frame = m->frame_count;
        m->frame_count = 0;
        while (m->running) {
            view_draw_splash(m);
            if (kbhit_custom()) {
                int ch = getch_custom();
                if (ch == 'q' || ch == 'Q') { m->running = 0; m->state = STATE_QUIT; break; }
                /* Qualquer outra tecla avança ao menu */
                break;
            }
            sleep_ms(60);
        }
        m->frame_count = saved_frame;
        view_clear();
    }

    while (m->running) {
        switch (m->state) {

        /* ── MENU ── */
        case STATE_MENU:
            controller_play_audio_once("pacman_intermission", "menu_music");
            view_reset_menu_state();
            view_clear();
            view_draw_menu(m);
            while (m->state == STATE_MENU && m->running) {
                controller_menu_input(m);
                sleep_ms(60);
                /* Anima menu (pisca ultima linha) */
                if (m->state == STATE_MENU) view_draw_menu(m);
            }
            controller_stop_audio_alias("menu_music");
            if (m->state == STATE_PLAYING) {
                m->score = 0;
                m->lives = START_LIVES;
                m->level = 1;
                m->score_history_count = 0;
                m->cumulative_ghosts_eaten = 0;
                m->cumulative_fruits_eaten = 0;
                memset(m->cumulative_ghosts_eaten_by_ghost, 0, sizeof(m->cumulative_ghosts_eaten_by_ghost));
                model_load_map(m);
                m->ready_timer = 0;
                view_clear();
                view_draw_map(m);
                view_draw_hud(m);
                view_draw_entities(m);
                view_flush();
            } else if (m->state == STATE_SCORES) {
                view_draw_scores(m);
                while (m->state == STATE_SCORES && m->running) {
                    if (kbhit_custom()) {
                        int ch = getch_custom();
                        if (ch == '\r' || ch == '\n' || ch == ' ' || ch == 27)
                            m->state = STATE_MENU;
                        if (ch == 'q' || ch == 'Q') {
                            m->running = 0; m->state = STATE_QUIT;
                        }
                    }
                    sleep_ms(50);
                }
            } else if (m->state == STATE_HELP) {
                view_draw_help(m);
                while (m->state == STATE_HELP && m->running) {
                    if (kbhit_custom()) {
                        int ch = getch_custom();
                        if (ch == '\r' || ch == '\n' || ch == ' ' || ch == 27)
                            m->state = STATE_MENU;
                        if (ch == 'q' || ch == 'Q') {
                            m->running = 0; m->state = STATE_QUIT;
                        }
                    }
                    sleep_ms(50);
                }
            }
            break;

        case STATE_SKIN_SELECT:
            view_clear();
            view_draw_skin_selector(m);
            while (m->state == STATE_SKIN_SELECT && m->running) {
                controller_skin_input(m);
                sleep_ms(60);
                if (m->state == STATE_SKIN_SELECT) view_draw_skin_selector(m);
            }
            if (m->state == STATE_PLAYING) {
                controller_stop_audio_alias("menu_music");
                m->score = 0;
                m->lives = START_LIVES;
                m->level = 1;
                m->score_history_count = 0;
                m->cumulative_ghosts_eaten = 0;
                m->cumulative_fruits_eaten = 0;
                memset(m->cumulative_ghosts_eaten_by_ghost, 0, sizeof(m->cumulative_ghosts_eaten_by_ghost));
                model_load_map(m);
                m->ready_timer = 0;
                view_clear();
                view_draw_map(m);
                view_draw_hud(m);
                view_draw_entities(m);
                view_flush();
                view_draw_ready();
                view_flush();
                controller_play_audio_and_wait("pacman_beginning", "start_music");
                view_clear_ready(m);
                view_flush();
            }
            break;

        case STATE_NAME_ENTRY:
            view_clear();
            view_draw_name_entry(m);
            while (m->state == STATE_NAME_ENTRY && m->running) {
                controller_name_input(m);
                sleep_ms(60);
                if (m->state == STATE_NAME_ENTRY) view_draw_name_entry(m);
            }
            break;

        /* ── JOGANDO ── */
        case STATE_PLAYING:
            if (!m->auto_play) controller_handle_input(m);
            else {
                /* No modo auto, ainda permite Q para sair */
                while (kbhit_custom()) {
                    int ch = getch_custom();
                    if (ch == 'q' || ch == 'Q') { m->running = 0; m->state = STATE_QUIT; }
                    /* M = volta para modo manual */
                    if (ch == 'm' || ch == 'M') {
                        m->auto_play = 0;
                        m->desired_dx = 0;
                        m->desired_dy = 0;
                    }
                    /* P = pausa */
                    if (ch == 'p' || ch == 'P') m->state = STATE_PAUSED;
                }
            }
            controller_update(m);
            sleep_ms(m->frame_ms);
            break;

        /* ── PAUSADO ── */
        case STATE_PAUSED: {
            int selected = 0; /* 0 = continuar, 1 = sair */
            view_draw_pause_menu(m, selected);
            while (m->state == STATE_PAUSED && m->running) {
                if (kbhit_custom()) {
                    int ch = getch_custom();
#ifdef _WIN32
                    if (ch == 0 || ch == 224) {
                        int arrow = getch_custom();
                        if (arrow == 72 || arrow == 75) selected = 0;
                        if (arrow == 80 || arrow == 77) selected = 1;
                        view_draw_pause_menu(m, selected);
                        continue;
                    }
#else
                    if (ch == 27 && kbhit_custom()) {
                        int c2 = getch_custom();
                        if (c2 == '[' && kbhit_custom()) {
                            int c3 = getch_custom();
                            if (c3 == 'A' || c3 == 'D') selected = 0;
                            if (c3 == 'B' || c3 == 'C') selected = 1;
                            view_draw_pause_menu(m, selected);
                        }
                        continue;
                    }
#endif
                    if (ch == 'w' || ch == 'W' || ch == 'a' || ch == 'A') selected = 0;
                    if (ch == 's' || ch == 'S' || ch == 'd' || ch == 'D') selected = 1;
                    if (ch == 'p' || ch == 'P') {
                        if (selected == 0) {
                            m->state = STATE_PLAYING;
                        } else {
                            m->pending_score = m->score;
                            m->pending_name_len = 0;
                            m->pending_name[0] = '\0';
                            m->state = STATE_NAME_ENTRY;
                        }
                    }
                    if (ch == 'q' || ch == 'Q' || ch == '\r' || ch == '\n' || ch == ' ') {
                        if (ch == 'q' || ch == 'Q') selected = 1;
                        if (selected == 0) {
                            m->state = STATE_PLAYING;
                        } else {
                            m->pending_score = m->score;
                            m->pending_name_len = 0;
                            m->pending_name[0] = '\0';
                            m->state = STATE_NAME_ENTRY;
                        }
                    }
                    if (m->state == STATE_PAUSED) view_draw_pause_menu(m, selected);
                }
                sleep_ms(50);
            }
            if (m->state == STATE_PLAYING) {
                /* Full redraw to remove pause overlay */
                view_draw_game(m);
            }
            break;
        }

        /* ── MORRENDO ── */
        case STATE_DYING:
            for (int f = 0; f < 14; f++) {
                view_draw_death_anim(m, f);
                view_flush();
                sleep_ms(90);
            }
            model_reset_positions(m);
            view_draw_map(m);
            view_draw_hud(m);
            view_draw_ready();
            view_draw_entities(m);
            view_flush();
            m->state = STATE_PLAYING;
            break;

        /* ── VITORIA ── */
        case STATE_WIN:
            view_draw_win(m);
            controller_play_audio_once("audio_tela_de_virotia", "win_sound");
            sleep_ms(2000);
            while (kbhit_custom()) getch_custom();
            while (1) {
                if (kbhit_custom()) {
                    int ch = getch_custom();
                    if (ch == '\r' || ch == '\n' || ch == ' ') break;
                    if (ch == 'q' || ch == 'Q') { m->running = 0; m->state = STATE_QUIT; break; }
                }
                sleep_ms(50);
            }
            if (m->running) {
                m->cumulative_ghosts_eaten += m->phase_ghosts_eaten;
                m->cumulative_fruits_eaten += m->phase_fruits_eaten;
                for (int i = 0; i < GHOSTS; i++) {
                    m->cumulative_ghosts_eaten_by_ghost[i] += m->ghosts[i].eaten;
                }
                m->level++;
                model_load_map(m);
                view_clear();
                view_draw_map(m);
                view_draw_hud(m);
                view_draw_entities(m);
                view_flush();
                view_draw_ready();
                view_flush();
                controller_stop_audio_alias("menu_music");
                controller_play_audio_and_wait("pacman_beginning", "start_music");
                view_clear_ready(m);
                view_flush();
                m->state = STATE_PLAYING;
            }
            break;

        /* ── GAME OVER ── */
        case STATE_GAMEOVER:
            view_draw_gameover(m);
            controller_play_audio_once("audio_tela_de_game_over", "gameover_sound");
            sleep_ms(1000);
            while (kbhit_custom()) getch_custom();
            while (1) {
                if (kbhit_custom()) {
                    int ch = getch_custom();
                    if (ch == '\r' || ch == '\n' || ch == ' ') {
                        m->state = STATE_MENU; break;
                    }
                    if (ch == 'h' || ch == 'H') {
                        m->state = STATE_SCORES;
                        view_draw_scores(m);
                        while (m->state == STATE_SCORES) {
                            if (kbhit_custom()) {
                                int c2 = getch_custom();
                                if (c2 == '\r' || c2 == '\n' || c2 == ' ' || c2 == 27)
                                    m->state = STATE_MENU;
                            }
                            sleep_ms(50);
                        }
                        break;
                    }
                    if (ch == 'q' || ch == 'Q') {
                        m->running = 0; m->state = STATE_QUIT; break;
                    }
                }
                sleep_ms(50);
            }
            break;

        case STATE_SCORES:
            m->state = STATE_MENU;
            break;

        case STATE_QUIT:
        default:
            m->running = 0;
            break;
        }
    }
}
static void controller_skin_input(GameModel *m) {
    while (kbhit_custom()) {
        int ch = getch_custom();
#ifdef _WIN32
        if (ch == 27) {
            m->state = STATE_MENU;
            return;
        }
        if (ch == 0 || ch == 224) {
            int arrow = getch_custom();
            if (arrow == 72 || arrow == 75) {
                m->skin_selected = (m->skin_selected - 1 + SKIN_COUNT) % SKIN_COUNT;
            } else if (arrow == 80 || arrow == 77) {
                m->skin_selected = (m->skin_selected + 1) % SKIN_COUNT;
            }
            continue;
        }
#else
        if (ch == 27) {
            if (kbhit_custom()) {
                int c2 = getch_custom();
                if (c2 == '[' && kbhit_custom()) {
                    int c3 = getch_custom();
                    if (c3 == 'A' || c3 == 'D') {
                        m->skin_selected = (m->skin_selected - 1 + SKIN_COUNT) % SKIN_COUNT;
                    } else if (c3 == 'B' || c3 == 'C') {
                        m->skin_selected = (m->skin_selected + 1) % SKIN_COUNT;
                    }
                } else {
                    m->state = STATE_MENU;
                    return;
                }
            } else {
                m->state = STATE_MENU;
                return;
            }
            continue;
        }
#endif
        if (ch == 'w' || ch == 'W' || ch == 'a' || ch == 'A') {
            m->skin_selected = (m->skin_selected - 1 + SKIN_COUNT) % SKIN_COUNT;
            continue;
        }
        if (ch == 's' || ch == 'S' || ch == 'd' || ch == 'D') {
            m->skin_selected = (m->skin_selected + 1) % SKIN_COUNT;
            continue;
        }
        if (ch == '\r' || ch == '\n' || ch == ' ') {
            m->state = STATE_PLAYING;
            return;
        }
        if (ch == 'q' || ch == 'Q') {
            m->running = 0;
            m->state = STATE_QUIT;
            return;
        }
    }
}

static void controller_name_input(GameModel *m) {
    while (kbhit_custom()) {
        int ch = getch_custom();
#ifdef _WIN32
        if (ch == 0 || ch == 224) {
            getch_custom();
            continue;
        }
#else
        if (ch == 27 && kbhit_custom()) {
            int c2 = getch_custom();
            if (c2 == '[' && kbhit_custom()) getch_custom();
            continue;
        }
#endif
        if (ch == 8 || ch == 127) {
            if (m->pending_name_len > 0) {
                m->pending_name[--m->pending_name_len] = '\0';
            }
            continue;
        }
        if (ch == '\r' || ch == '\n') {
            if (m->pending_name_len == 0) {
                strncpy(m->pending_name, "YOU", MAX_NAME_LEN);
                m->pending_name_len = 3;
            }
            m->score_tree = bst_insert(m->score_tree, m->pending_score, m->pending_name);
            if (bst_count(m->score_tree) > 10) {
                BSTNode *mn = bst_min(m->score_tree);
                if (mn) m->score_tree = bst_remove(m->score_tree, mn->score);
            }
            model_save_scores(m);
            m->state = STATE_GAMEOVER;
            return;
        }
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') ||
            (ch >= '0' && ch <= '9') || ch == '_' || ch == '-') {
            if (m->pending_name_len < MAX_NAME_LEN) {
                m->pending_name[m->pending_name_len++] = (char)ch;
                m->pending_name[m->pending_name_len] = '\0';
            }
        }
    }
}
/* ======================== MAIN ======================== */

int main(void) {
    srand((unsigned)time(NULL));

    GameModel model;
    model_init(&model);

    view_init();
    controller_run(&model);

    /* Tela de saida */
    view_clear();
    printf("\n");
    printf("  %s╔══════════════════════════════════════╗%s\n", CLR_WALL, CLR_RESET);
    printf("  %s║%s  %sObrigado por jogar PAC-MAN!%s        %s║%s\n",
           CLR_WALL, CLR_RESET, CLR_TITLE, CLR_RESET, CLR_WALL, CLR_RESET);
    printf("  %s║%s  Score final : %s%-8d%s               %s║%s\n",
           CLR_WALL, CLR_RESET, CLR_SCORE, model.score, CLR_RESET, CLR_WALL, CLR_RESET);
    printf("  %s║%s  High Score  : %s%-8d%s               %s║%s\n",
           CLR_WALL, CLR_RESET, CLR_CYAN, model.high_score, CLR_RESET, CLR_WALL, CLR_RESET);
    if (model.auto_play) {
        printf("  %s║%s  %s[Auto-Play]%s Pellets: %-4d  Ghosts: %-2d  %s║%s\n",
               CLR_WALL, CLR_RESET, CLR_AUTO_TAG, CLR_RESET,
               model.auto_pellets_eaten, model.auto_ghosts_eaten,
               CLR_WALL, CLR_RESET);
    }
    printf("  %s╚══════════════════════════════════════╝%s\n\n", CLR_WALL, CLR_RESET);

    model_free(&model);
    view_cleanup();

    return 0;
}
