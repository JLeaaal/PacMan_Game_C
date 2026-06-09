/*
 * ============================================================
 *  VIEW — Renderização ANSI nível arcade Nintendo
 * ============================================================
 *  Reescrito do zero para qualidade profissional. Tudo em C
 *  com ANSI puro (sem ncurses, sem dependência externa):
 *
 *  - Paredes com auto-tiling (cantos arredondados, T, blocos)
 *  - Pac-Man com 4 frames de animação e boca direcional
 *  - Fantasmas com indicador de direção, piscar branco no fim
 *  - Power pellet com respiração (radiance pulse)
 *  - Splash arcade com logo gigante e "Pressione ENTER"
 *  - Menu redesenhado com sombra 3D e seletor de mapas
 *  - HUD lateral em estilo arcade cabinet com:
 *      - Gauge gráfico do power-up (barra que diminui)
 *      - Combo grande, cards dos fantasmas com IA
 *      - Estruturas em tempo real (BST/AVL/Graph)
 *      - Barra de progresso de pellets
 *  - Bonus floats (+200, +400...) sobem suavemente
 *  - Animação de morte estendida (9 frames)
 *  - Tela de vitória com flash + benchmark de sorts
 *  - High Scores com pódio para top 3
 * ============================================================
 */

#include "pacman.h"

#ifndef _WIN32
#include <sys/ioctl.h>
#endif

/* ======================== PLATAFORMA ======================== */

#ifndef _WIN32
static struct termios orig_termios;
#endif

void sleep_ms(int ms) {
#ifdef _WIN32
    Sleep(ms);
#else
    usleep(ms * 1000);
#endif
}

void enable_ansi(void) {
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    GetConsoleMode(hOut, &mode);
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, mode);
    SetConsoleOutputCP(65001);
#endif
}

int kbhit_custom(void) {
#ifdef _WIN32
    return _kbhit();
#else
    int ch = getchar();
    if (ch != EOF) { ungetc(ch, stdin); return 1; }
    return 0;
#endif
}

int getch_custom(void) {
#ifdef _WIN32
    return _getch();
#else
    return getchar();
#endif
}

int manhattan(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

/* ======================== TERMINAL ======================== */

static void move_cursor(int x, int y) { printf("\033[%d;%dH", y + 1, x + 1); }
static void hide_cursor(void)         { printf("\033[?25l"); }
static void show_cursor(void)         { printf("\033[?25h"); }
static void reset_cursor(void)        { printf("\033[H"); }

void view_clear(void) { printf("\033[2J\033[H"); }
void view_flush(void) { fflush(stdout); }

static int terminal_width(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return 120;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return 120;
    return csbi.srWindow.Right - csbi.srWindow.Left + 1;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) return w.ws_col;
    return 120;
#endif
}

static int terminal_height(void) {
#ifdef _WIN32
    CONSOLE_SCREEN_BUFFER_INFO csbi;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE) return 40;
    if (!GetConsoleScreenBufferInfo(hOut, &csbi)) return 40;
    return csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
#else
    struct winsize w;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_row > 0) return w.ws_row;
    return 40;
#endif
}

int view_offset_x(void) {
    int game_w = MAP_W * CELL_W + 4 + 22;
    int width = terminal_width();
    int left = (width > game_w) ? (width - game_w) / 2 : 2;
    return left > 0 ? left : 2;
}

int view_offset_y(void) {
    int game_h = MAP_H + 6;
    int height = terminal_height();
    int top = (height > game_h) ? (height - game_h) / 2 : 6;
    return top > 5 ? top : 6;
}

static int view_center_x(int width) {
    int term_w = terminal_width();
    int x = (term_w > width) ? (term_w - width) / 2 : 1;
    return x > 0 ? x : 1;
}

static int view_center_y(int height) {
    int term_h = terminal_height();
    int y = (term_h > height) ? (term_h - height) / 2 : 1;
    return y > 0 ? y : 1;
}

void view_init(void) {
    enable_ansi();
    setvbuf(stdout, NULL, _IOFBF, 1 << 20);
#ifndef _WIN32
    tcgetattr(STDIN_FILENO, &orig_termios);
    struct termios raw = orig_termios;
    raw.c_lflag &= ~(ICANON | ECHO);
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw);
    fcntl(STDIN_FILENO, F_SETFL, O_NONBLOCK);
#endif
    hide_cursor();
    printf("\033[40m"); /* fundo preto */
}

void view_cleanup(void) {
    show_cursor();
    printf(CLR_RESET "\033[49m");
#ifndef _WIN32
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
#endif
}

/* ======================== PRIMITIVAS ======================== */

static void print_abs(int col, int row, const char *color, const char *str) {
    move_cursor(col, row);
    printf("%s%s%s", color, str, CLR_RESET);
}

static void clear_rect(int col, int row, int w, int h) {
    for (int dy = 0; dy < h; dy++) {
        move_cursor(col, row + dy);
        for (int dx = 0; dx < w; dx++) printf(" ");
    }
}

/* ======================== AUTO-TILING DA PAREDE ======================== */
/*
 * O tile da parede é escolhido com base nos vizinhos cardeais.
 * Resultado: cantos arredondados, blocos sólidos e linhas — o
 * labirinto fica parecido com o gabinete original do Pac-Man,
 * em vez de "▓▓" repetido.
 */

static int is_wall_or_door(GameModel *m, int x, int y) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return 1; /* fora = parede */
    int t = m->grid[y][x];
    return (t == TILE_WALL || t == TILE_DOOR);
}

static const char *wall_glyph(GameModel *m, int x, int y) {
    int u = is_wall_or_door(m, x, y - 1);
    int d = is_wall_or_door(m, x, y + 1);
    int l = is_wall_or_door(m, x - 1, y);
    int r = is_wall_or_door(m, x + 1, y);

    /* Núcleo: cercado em todos os lados → bloco sólido */
    if (u && d && l && r) return "██";

    /* Lados (T-junções) → bloco sólido pra vedar */
    if (u && d && (l || r)) return "██";
    if (l && r && (u || d)) return "██";

    /* Cantos arredondados (estilo arcade) */
    if (d && r && !u && !l) return "╭─";
    if (d && l && !u && !r) return "─╮";
    if (u && r && !d && !l) return "╰─";
    if (u && l && !d && !r) return "─╯";

    /* Linhas finas isoladas */
    if (l && r) return "──";
    if (u && d) return "││";

    /* Ponta única */
    if (l)         return "─ ";
    if (r)         return " ─";
    if (u || d)    return "││";

    return "██";
}

/* ======================== CÉLULAS DO MAPA ======================== */

void view_draw_cell(GameModel *m, int x, int y) {
    int tile = m->grid[y][x];
    int sx = OFFSET_X + x * CELL_W;
    int sy = OFFSET_Y + y;
    move_cursor(sx, sy);

    switch (tile) {
        case TILE_WALL: {
            const char *g = wall_glyph(m, x, y);
            const char *wall_color = (m->skin_selected == 1) ? CLR_MAGENTA : CLR_WALL;
            printf("%s%s%s", wall_color, g, CLR_RESET);
            break;
        }
        case TILE_PELLET:
            printf("%s ·%s", CLR_PELLET, CLR_RESET);
            break;
        case TILE_POWER: {
            /* Power pellet "respira" em 4 fases visuais */
            int ph = (m->frame_count / 3) % 8;
            const char *glyph; const char *color;
            if      (ph < 2) { glyph = " •"; color = CLR_POWER; }
            else if (ph < 4) { glyph = " ●"; color = CLR_POWER; }
            else if (ph < 6) { glyph = " ◉"; color = CLR_ORANGE; }
            else             { glyph = " ●"; color = CLR_POWER; }
            printf("%s%s%s", color, glyph, CLR_RESET);
            break;
        }
        case TILE_DOOR:
            printf("%s══%s", CLR_DOOR, CLR_RESET);
            break;
        default:
            printf("  ");
            break;
    }
}

void view_draw_map(GameModel *m) {
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            view_draw_cell(m, x, y);

    /* Moldura externa em estilo arcade cabinet */
    int left   = OFFSET_X - 2;
    int top    = OFFSET_Y - 1;
    int right  = OFFSET_X + MAP_W * CELL_W;
    int bottom = OFFSET_Y + MAP_H;

    const char *hi_color = (m->skin_selected == 1) ? CLR_MAGENTA : CLR_WALL_HI;

    move_cursor(left, top);
    printf("%s╔", hi_color);
    for (int i = 0; i < MAP_W * CELL_W + 2; i++) printf("═");
    printf("╗%s", CLR_RESET);

    for (int row = top + 1; row < bottom + 1; row++) {
        move_cursor(left,      row);  printf("%s║%s", hi_color, CLR_RESET);
        move_cursor(right + 1, row);  printf("%s║%s", hi_color, CLR_RESET);
    }

    move_cursor(left, bottom + 1);
    printf("%s╚", hi_color);
    for (int i = 0; i < MAP_W * CELL_W + 2; i++) printf("═");
    printf("╝%s", CLR_RESET);
}

/* ======================== ENTIDADES — PAC-MAN ======================== */

static const char *pacman_left_char(GameModel *m) {
    int anim = (m->frame_count / 3) % 4;
    int dx = m->current_dx, dy = m->current_dy;
    if (dx == 0 && dy == 0) dx = 1;
    int is_ms = (m->skin_selected == 1);

    if (anim == 0) return "●";

    if (anim == 1 || anim == 3) {
        if (dx > 0)  return "ᗧ";
        if (dx < 0)  return is_ms ? "❀" : "•";
        if (dy < 0)  return "ᗢ";
        return "ᗣ";
    }

    if (dx > 0)  return "ᗧ";
    if (dx < 0)  return is_ms ? "❀" : " ";
    if (dy < 0)  return "ᗢ";
    return "ᗣ";
}

static const char *pacman_right_char(GameModel *m) {
    int anim = (m->frame_count / 3) % 4;
    int dx = m->current_dx, dy = m->current_dy;
    if (dx == 0 && dy == 0) dx = 1;
    int is_ms = (m->skin_selected == 1);

    if (anim == 0) return "●";

    if (anim == 1 || anim == 3) {
        if (dx > 0)  return is_ms ? "❀" : "•";
        if (dx < 0)  return "ᗤ";
        if (dy < 0)  return is_ms ? "❀" : " ";
        return is_ms ? "❀" : " ";
    }

    if (dx > 0)  return is_ms ? "❀" : " ";
    if (dx < 0)  return "ᗤ";
    if (dy < 0)  return is_ms ? "❀" : " ";
    return is_ms ? "❀" : " ";
}

/* ======================== ENTIDADES — FANTASMAS ======================== */

static const char *ghost_sprite(Ghost *g, int frame) {
    if (g->vulnerable) {
        return (frame % 6 < 3) ? "ᗣ▒" : "◌◌";
    }
    if (g->e.dx > 0)  return "ᗣ›";
    if (g->e.dx < 0)  return "‹ᗣ";
    if (g->e.dy < 0)  return "ᗣˆ";
    if (g->e.dy > 0)  return "ᗣˇ";
    return "ᗣ ";
}

static const char *ghost_color(int i, Ghost *g, int frame) {
    if (g->vulnerable) {
        return (frame % 8 < 4) ? CLR_VULN : CLR_VULN2;
    }
    switch (i) {
        case 0: return CLR_BLINKY;
        case 1: return CLR_PINKY;
        case 2: return CLR_INKY;
        case 3: return CLR_CLYDE;
    }
    return CLR_RESET;
}

void view_draw_entities(GameModel *m) {
    for (int i = 0; i < GHOSTS; i++) {
        if (m->ghosts[i].in_house && m->ghosts[i].release_timer > 0) continue;
        Ghost *g = &m->ghosts[i];
        move_cursor(OFFSET_X + g->e.x * CELL_W, OFFSET_Y + g->e.y);
        printf("%s%s%s", ghost_color(i, g, m->frame_count),
               ghost_sprite(g, m->frame_count), CLR_RESET);
    }
    move_cursor(OFFSET_X + m->pacman.x * CELL_W, OFFSET_Y + m->pacman.y);
    const char *left = pacman_left_char(m);
    const char *right = pacman_right_char(m);
    /* Ms. Pac-Man: keep body yellow but render bow in red */
    if (m->skin_selected == 1) {
        /* Bow may be on left or right depending on direction/animation */
        if (strcmp(left, "❀") == 0) {
            printf("%s%s%s%s%s", CLR_RED, left, CLR_PACMAN, right, CLR_RESET);
        } else if (strcmp(right, "❀") == 0) {
            printf("%s%s%s%s%s", CLR_PACMAN, left, CLR_RED, right, CLR_RESET);
        } else {
            printf("%s%s%s%s", CLR_PACMAN, left, right, CLR_RESET);
        }
    } else {
        const char *pc = model_skin_color(m->skin_selected);
        printf("%s%s%s%s", pc, left, right, CLR_RESET);
    }
}

void view_erase_entities(GameModel *m) {
    view_draw_cell(m, m->pacman.x, m->pacman.y);
    for (int i = 0; i < GHOSTS; i++) {
        if (m->ghosts[i].in_house && m->ghosts[i].release_timer > 0) continue;
        view_draw_cell(m, m->ghosts[i].e.x, m->ghosts[i].e.y);
    }
}

/* ======================== HEADER SUPERIOR ======================== */

static void view_draw_header(GameModel *m) {
    int center = OFFSET_X + MAP_W * CELL_W / 2;

    /* 1UP */
    move_cursor(OFFSET_X, 1);
    printf("%s 1UP %s", CLR_DIM, CLR_RESET);
    move_cursor(OFFSET_X + 1, 2);
    printf("%s%6d%s", CLR_SCORE, m->score, CLR_RESET);

    /* HIGH SCORE central */
    move_cursor(center - 6, 1);
    printf("%sHIGH SCORE%s", CLR_DIM, CLR_RESET);
    move_cursor(center - 3, 2);
    printf("%s%6d%s", CLR_TITLE, m->high_score, CLR_RESET);

    /* NIVEL à direita */
    int right = OFFSET_X + MAP_W * CELL_W - 8;
    move_cursor(right, 1);
    printf("%sNIVEL%s", CLR_DIM, CLR_RESET);
    move_cursor(right + 1, 2);
    printf("%s  %02d %s", CLR_CYAN, m->level, CLR_RESET);

    move_cursor(right - 10, 1);
    printf("%sSKIN%s", CLR_DIM, CLR_RESET);
    move_cursor(right - 9, 2);
    printf("%s%-7s%s", CLR_TITLE, model_skin_name(m->skin_selected), CLR_RESET);

    /* Separador fino */
    move_cursor(OFFSET_X - 2, 3);
    printf("%s", CLR_DIM2);
    for (int i = 0; i < MAP_W * CELL_W + 4; i++) printf("─");
    printf("%s", CLR_RESET);

    /* Linha de vidas + badge auto-play */
    move_cursor(OFFSET_X, 4);
    for (int i = 0; i < MAP_W * CELL_W; i++) printf(" ");
    move_cursor(OFFSET_X, 4);
    printf("%sVIDAS%s ", CLR_DIM, CLR_RESET);
    for (int i = 0; i < m->lives - 1 && i < 5; i++)
        printf("%sᗧ %s", CLR_PACMAN, CLR_RESET);

    if (m->auto_play) {
        move_cursor(center - 7, 4);
        int blink = (m->frame_count / 8) % 2;
        if (blink)
            printf("%s ▶ AUTO-PLAY ◀ %s", CLR_AUTO_TAG, CLR_RESET);
        else
            printf("%s   AUTO-PLAY   %s", CLR_GREEN, CLR_RESET);
    }
}

/* ======================== PAINEL LATERAL (arcade cabinet) ======================== */

#define PANEL_X  (OFFSET_X + MAP_W * CELL_W + 4)
#define PANEL_W  22

static void panel_top(int row) {
    move_cursor(PANEL_X, row);
    printf("%s╭", CLR_PANEL_BORDER);
    for (int i = 0; i < PANEL_W - 2; i++) printf("─");
    printf("╮%s", CLR_RESET);
}

static void panel_bottom(int row) {
    move_cursor(PANEL_X, row);
    printf("%s╰", CLR_PANEL_BORDER);
    for (int i = 0; i < PANEL_W - 2; i++) printf("─");
    printf("╯%s", CLR_RESET);
}

static void panel_sep(int row) {
    move_cursor(PANEL_X, row);
    printf("%s├", CLR_PANEL_BORDER);
    for (int i = 0; i < PANEL_W - 2; i++) printf("─");
    printf("┤%s", CLR_RESET);
}

static void panel_text(int row, const char *color, const char *text) {
    move_cursor(PANEL_X, row);
    printf("%s│%s", CLR_PANEL_BORDER, CLR_RESET);
    move_cursor(PANEL_X + 1, row);
    printf("                    "); /* limpa */
    move_cursor(PANEL_X + 2, row);
    printf("%s%s%s", color, text, CLR_RESET);
    move_cursor(PANEL_X + PANEL_W - 1, row);
    printf("%s│%s", CLR_PANEL_BORDER, CLR_RESET);
}

static void panel_header(int row, const char *text) {
    move_cursor(PANEL_X, row);
    printf("%s│%s", CLR_PANEL_BORDER, CLR_RESET);
    move_cursor(PANEL_X + 1, row);
    printf("%s%s", CLR_SEL_BG, CLR_PANEL_HDR);
    int len = (int)strlen(text);
    int pad = (PANEL_W - 2 - len) / 2;
    if (pad < 0) pad = 0;
    for (int i = 0; i < pad; i++) printf(" ");
    printf("%s", text);
    int rem = PANEL_W - 2 - pad - len;
    if (rem < 0) rem = 0;
    for (int i = 0; i < rem; i++) printf(" ");
    printf("%s", CLR_RESET);
    move_cursor(PANEL_X + PANEL_W - 1, row);
    printf("%s│%s", CLR_PANEL_BORDER, CLR_RESET);
}

/* Gauge horizontal (barra que vai diminuindo) */
static void panel_gauge(int row, const char *label, int value, int max, const char *color) {
    move_cursor(PANEL_X, row);
    printf("%s│%s ", CLR_PANEL_BORDER, CLR_RESET);
    printf("%s%-5s%s ", CLR_DIM, label, CLR_RESET);

    int bar_w = PANEL_W - 10;
    int filled = (max > 0) ? (value * bar_w) / max : 0;
    if (filled < 0)      filled = 0;
    if (filled > bar_w)  filled = bar_w;

    printf("%s", color);
    for (int i = 0; i < filled; i++)         printf("█");
    printf("%s", CLR_DIM2);
    for (int i = filled; i < bar_w; i++)     printf("░");
    printf("%s ", CLR_RESET);

    move_cursor(PANEL_X + PANEL_W - 1, row);
    printf("%s│%s", CLR_PANEL_BORDER, CLR_RESET);
}

/* Card de fantasma: cor + sprite + nome + algoritmo */
static void panel_ghost_card(int row, const char *clr, const char *name, const char *ai) {
    move_cursor(PANEL_X, row);
    printf("%s│%s ", CLR_PANEL_BORDER, CLR_RESET);
    printf("%sᗣ%s %s%-7s%s %s%-8s%s",
           clr, CLR_RESET,
           clr, name, CLR_RESET,
           CLR_DIM, ai, CLR_RESET);
    move_cursor(PANEL_X + PANEL_W - 1, row);
    printf("%s│%s", CLR_PANEL_BORDER, CLR_RESET);
}

void view_draw_side_panel(GameModel *m) {
    int r = OFFSET_Y - 1;

    panel_top(r++);
    panel_header(r++, "PAC-MAN  ARCADE");
    panel_sep(r++);

    /* ---- Power gauge ---- */
    panel_header(r++, "POWER");
    if (m->power_timer > 0) {
        char buf[48];
        snprintf(buf, sizeof(buf), "%2ds restante", m->power_timer / 12);
        panel_text(r++, CLR_POWER, buf);
        panel_gauge(r++, "GAUGE", m->power_timer, POWER_TIME, CLR_POWER);
    } else {
        panel_text(r++, CLR_DIM, "inativo");
        panel_gauge(r++, "GAUGE", 0, POWER_TIME, CLR_POWER);
    }
    panel_sep(r++);

    /* ---- Combo grande ---- */
    {
        panel_header(r++, "COMBO");
        char buf[48];
        if (m->ghost_eat_combo > 0)
            snprintf(buf, sizeof(buf), " x%d  +%d pts",
                     m->ghost_eat_combo, 200 << (m->ghost_eat_combo - 1));
        else
            snprintf(buf, sizeof(buf), " x0  (sem combo)");
        panel_text(r++, m->ghost_eat_combo > 0 ? CLR_ORANGE : CLR_DIM, buf);
    }
    panel_sep(r++);

    /* ---- Cards dos fantasmas ---- */
    panel_header(r++, "FANTASMAS / IA");
    panel_ghost_card(r++, CLR_BLINKY, "Blinky", "Dijkstra");
    panel_ghost_card(r++, CLR_PINKY,  "Pinky",  "BFS");
    panel_ghost_card(r++, CLR_INKY,   "Inky",   "DFS");
    panel_ghost_card(r++, CLR_CLYDE,  "Clyde",  "Manhatt.");
    panel_sep(r++);

    /* ---- Estruturas ---- */
    panel_header(r++, "ESTRUTURAS");
    {
        char buf[48];
        snprintf(buf, sizeof(buf), "BST  h=%-2d  n=%-3d",
                 bst_height(m->score_tree), bst_count(m->score_tree));
        panel_text(r++, CLR_CYAN, buf);

        snprintf(buf, sizeof(buf), "AVL  h=%-2d  b=%-2d",
                 avl_height(m->powerup_tree), avl_balance_factor(m->powerup_tree));
        panel_text(r++, CLR_CYAN, buf);

        int edges = 0;
        if (m->maze_graph) {
            for (int i = 0; i < m->maze_graph->num_vertices; i++) {
                AdjNode *cur = m->maze_graph->adj_list[i];
                while (cur) { edges++; cur = cur->next; }
            }
        }
        snprintf(buf, sizeof(buf), "Graph  E=%-4d", edges);
        panel_text(r++, CLR_CYAN, buf);
    }
    panel_sep(r++);

    /* ---- DOTS ---- */
    panel_header(r++, "DOTS");
    panel_gauge(r++, "MAP",
                m->total_pellets - m->pellets_left,
                m->total_pellets > 0 ? m->total_pellets : 1,
                CLR_PELLET);
    {
        char buf[48];
        snprintf(buf, sizeof(buf), "restam: %d", m->pellets_left);
        panel_text(r++, CLR_PELLET, buf);
    }
    panel_sep(r++);

    /* ---- Auto-play stats ---- */
    if (m->auto_play) {
        panel_header(r++, "AUTO-PLAY");
        char buf[48];
        snprintf(buf, sizeof(buf), "pellets:  %3d", m->auto_pellets_eaten);
        panel_text(r++, CLR_AUTO_TAG, buf);
        snprintf(buf, sizeof(buf), "ghosts :  %3d", m->auto_ghosts_eaten);
        panel_text(r++, CLR_AUTO_TAG, buf);
        char st[16];
        strncpy(st, m->auto_status, 14); st[14] = '\0';
        snprintf(buf, sizeof(buf), "modo: %s", st);
        panel_text(r++, CLR_GREEN, buf);
        panel_sep(r++);
    }

    /* ---- Atalhos ---- */
    panel_header(r++, "CONTROLES");
    panel_text(r++, CLR_DIM, "WASD/setas mover");
    panel_text(r++, CLR_DIM, "P  pausar");
    panel_text(r++, CLR_DIM, "Q  sair");

    panel_bottom(r);
}

void view_draw_hud(GameModel *m) {
    view_draw_header(m);
    view_draw_side_panel(m);
}

static void draw_arcade_loop(GameModel *m, int bx, int by, int width, int *phase, int *frames) {
    /* Limpa a linha da animação antes de redesenhar para evitar duplicações */
    clear_rect(bx, by, width, 1);

    int pac_x = 0, blinky = 0, pinky = 0, inky = 0, clyde = 0, pellet_x = 0;
    int progress = (m->frame_count / 2) % (width + 12);
    int chase_gap = width - 14;
    if (chase_gap < 10) chase_gap = 10;

    pellet_x = bx + 6 + (progress % chase_gap);

    if (*phase == 0) {
        pac_x = bx + 4 + progress;
        blinky = bx + 4 + progress - 3;
        pinky  = bx + 4 + progress - 5;
        inky   = bx + 4 + progress - 7;
        clyde  = bx + 4 + progress - 9;

        if (pac_x >= pellet_x - 1 && pac_x <= pellet_x + 1) {
            *phase = 1;
            *frames = 0;
        }
    } else {
        (*frames)++;
        int flee_t = *frames;
        pac_x = bx + 8 + flee_t;
        blinky = bx + 2 + (width - 10) - flee_t;
        pinky  = blinky - 2;
        inky   = blinky - 4;
        clyde  = blinky - 6;

        if (pac_x > bx + width - 4) {
            *phase = 0;
            *frames = 0;
        }
    }

    if (pellet_x >= bx + 4 && pellet_x < bx + width - 4) {
        move_cursor(pellet_x, by);
        printf("%s ●%s", CLR_POWER, CLR_RESET);
    }

    if (clyde  > bx + 4 && clyde  < bx + width - 4) {
        move_cursor(clyde, by);
        if (*phase == 1) printf("%s%s%s", CLR_VULN, ((m->frame_count / 4) % 2) ? "◌◌" : "ᗣ▒", CLR_RESET);
        else printf("%sᗣ %s", CLR_CLYDE, CLR_RESET);
    }
    if (inky   > bx + 4 && inky   < bx + width - 4) {
        move_cursor(inky, by);
        if (*phase == 1) printf("%s%s%s", CLR_VULN2, ((m->frame_count / 4) % 2) ? "◌◌" : "ᗣ▒", CLR_RESET);
        else printf("%sᗣ %s", CLR_INKY, CLR_RESET);
    }
    if (pinky  > bx + 4 && pinky  < bx + width - 4) {
        move_cursor(pinky, by);
        if (*phase == 1) printf("%s%s%s", CLR_VULN2, ((m->frame_count / 4) % 2) ? "◌◌" : "ᗣ▒", CLR_RESET);
        else printf("%sᗣ %s", CLR_PINKY, CLR_RESET);
    }
    if (blinky > bx + 4 && blinky < bx + width - 4) {
        move_cursor(blinky, by);
        if (*phase == 1) printf("%s%s%s", CLR_VULN, ((m->frame_count / 4) % 2) ? "◌◌" : "ᗣ▒", CLR_RESET);
        else printf("%sᗣ %s", CLR_BLINKY, CLR_RESET);
    }
    if (pac_x > bx + 4 && pac_x < bx + width - 4) {
        const char *sp = ((m->frame_count / 3) % 2) ? "ᗧ " : "● ";
        move_cursor(pac_x, by);
        printf("%s%s%s", CLR_PACMAN, sp, CLR_RESET);
    }
}

/* ======================== MENSAGENS NO MAPA ======================== */

void view_draw_ready(void) {
    int col = OFFSET_X + (MAP_W * CELL_W / 2) - 6;
    int row = OFFSET_Y + 17;
    move_cursor(col, row);
    printf("%s┌────────────┐%s", CLR_READY, CLR_RESET);
    move_cursor(col, row + 1);
    printf("%s│   READY!   │%s", CLR_READY, CLR_RESET);
    move_cursor(col, row + 2);
    printf("%s└────────────┘%s", CLR_READY, CLR_RESET);
}

void view_clear_ready(void) {
    int col = OFFSET_X + (MAP_W * CELL_W / 2) - 6;
    int row = OFFSET_Y + 17;
    for (int dy = 0; dy < 3; dy++) {
        move_cursor(col, row + dy);
        printf("              ");
    }
}

/* ======================== ANIMAÇÃO DE MORTE ======================== */

void view_draw_death_anim(GameModel *m, int frame) {
    const char *frames[] = {
        "ᗧ ", "◔ ", "◑ ", "◕ ", "● ", "◉ ", "○ ", "· ", "  "
    };
    int idx = frame / 2;
    if (idx >= 9) idx = 8;
    const char *colors[] = {
        CLR_PACMAN, CLR_PACMAN, CLR_PACMAN_SHADOW, CLR_ORANGE,
        CLR_ORANGE, CLR_GAMEOVER, CLR_GAMEOVER, CLR_DIM, CLR_DIM
    };
    move_cursor(OFFSET_X + m->pacman.x * CELL_W, OFFSET_Y + m->pacman.y);
    printf("%s%s%s", colors[idx], frames[idx], CLR_RESET);
}

/* ======================== BONUS FLOATS ======================== */

void view_restore_bonus_cell(GameModel *m, int x, int y) {
    if (x < 0 || x >= MAP_W || y < 0 || y >= MAP_H) return;
    int sx = OFFSET_X + x * CELL_W;
    int sy = OFFSET_Y + y;
    clear_rect(sx, sy, 8, 1);
    move_cursor(sx, sy);
    view_draw_cell(m, x, y);
}

void view_draw_bonus(GameModel *m) {
    if (m->bonus_timer <= 0 || m->bonus_points <= 0) return;

    int sx = OFFSET_X + m->bonus_x * CELL_W;
    int sy = OFFSET_Y + m->bonus_y;
    if (sy < OFFSET_Y) sy = OFFSET_Y;
    clear_rect(sx, sy, 8, 1);
    move_cursor(sx, sy);
    const char *col = (m->bonus_points >= 800)  ? CLR_TITLE :
                      (m->bonus_points >= 400)  ? CLR_ORANGE :
                      (m->bonus_points >= 200)  ? CLR_CYAN :
                                                  CLR_GREEN;
    printf("%s+%d%s", col, m->bonus_points, CLR_RESET);
}

void view_draw_game(GameModel *m) {
    reset_cursor();
    view_draw_map(m);
    view_draw_hud(m);
    view_draw_entities(m);
    view_flush();
}

/* ======================== CAIXAS DECORATIVAS ======================== */

static void draw_box_double(int col, int row, int w, int h, const char *color) {
    move_cursor(col, row);
    printf("%s╔", color);
    for (int i = 0; i < w - 2; i++) printf("═");
    printf("╗%s", CLR_RESET);

    for (int r = row + 1; r < row + h - 1; r++) {
        move_cursor(col, r);             printf("%s║%s", color, CLR_RESET);
        move_cursor(col + w - 1, r);     printf("%s║%s", color, CLR_RESET);
    }

    move_cursor(col, row + h - 1);
    printf("%s╚", color);
    for (int i = 0; i < w - 2; i++) printf("═");
    printf("╝%s", CLR_RESET);
}

static void draw_box_round(int col, int row, int w, int h, const char *color) {
    move_cursor(col, row);
    printf("%s╭", color);
    for (int i = 0; i < w - 2; i++) printf("─");
    printf("╮%s", CLR_RESET);

    for (int r = row + 1; r < row + h - 1; r++) {
        move_cursor(col, r);             printf("%s│%s", color, CLR_RESET);
        move_cursor(col + w - 1, r);     printf("%s│%s", color, CLR_RESET);
    }

    move_cursor(col, row + h - 1);
    printf("%s╰", color);
    for (int i = 0; i < w - 2; i++) printf("─");
    printf("╯%s", CLR_RESET);
}

/* Caixa com sombra (efeito 3D) — usada em menu/splash/scores/help */
static void draw_box_shadow(int col, int row, int w, int h) {
    /* Sombra abaixo/direita */
    for (int r = row + 1; r < row + h; r++) {
        move_cursor(col + w, r);
        printf("%s ▓%s", CLR_DIM2, CLR_RESET);
    }
    move_cursor(col + 1, row + h);
    printf("%s", CLR_DIM2);
    for (int i = 0; i < w + 1; i++) printf("▓");
    printf("%s", CLR_RESET);

    draw_box_double(col, row, w, h, CLR_WALL_HI);
    if (w >= 6 && h >= 6)
        draw_box_round(col + 1, row + 1, w - 2, h - 2, CLR_PANEL_BORDER);
}

static void draw_divider(int col, int row, int w) {
    move_cursor(col, row);
    printf("%s╠", CLR_WALL_HI);
    for (int i = 0; i < w - 2; i++) printf("═");
    printf("╣%s", CLR_RESET);
}

static int menu_drawn = 0;
static int menu_last_sel = -1;
static int menu_last_map = -1;
static int menu_last_blink = -1;
static int menu_last_anim = -1;

void view_reset_menu_state(void) {
    menu_drawn = 0;
    menu_last_sel = -1;
    menu_last_map = -1;
    menu_last_blink = -1;
    menu_last_anim = -1;
}

/* ======================== SPLASH ARCADE ======================== */
/*
 * Tela inicial estilo arcade clássico: logo gigante, tabela de
 * pontos, atract-mode (Pac-Man fugindo dos fantasmas) e
 * "PRESSIONE ENTER" piscando. Qualquer tecla avança ao menu.
 */
void view_draw_splash(GameModel *m) {
    static int drawn = 0;

    if (!drawn) {
        view_clear();
        reset_cursor();
        drawn = 1;
    }

    int W = 74, H = 28;
    int bx = view_center_x(W), by = view_center_y(H);

    int blink = (m->frame_count / 10) % 2;
    int press_y = by + H / 2;
    clear_rect(bx + 4, press_y, W - 8, 1);
    if (blink) {
        const char *msg = "PRESSIONE ENTER PARA COMECAR";
        move_cursor(bx + (W - (int)strlen(msg)) / 2, press_y);
        printf("%s%s%s", CLR_TITLE, msg, CLR_RESET);
    }

    view_flush();
    m->frame_count++;
}

/* ======================== MENU PRINCIPAL (com seletor de mapas) ======================== */

static void menu_item_styled(int x, int y, int selected, const char *text, const char *hotkey, int width) {
    move_cursor(x, y);
    if (selected) {
        printf("%s%s ▶ %s%-*s   %s%s%s",
               CLR_SEL_BG, CLR_TITLE,
               CLR_TITLE, width, text,
               CLR_DIM, hotkey ? hotkey : "",
               CLR_RESET);
    } else {
        printf("   %s%-*s%s   %s%s%s",
               CLR_SUBTITLE, width, text, CLR_RESET,
               CLR_DIM2, hotkey ? hotkey : "",
               CLR_RESET);
    }
}

void view_draw_menu(GameModel *m) {
    int bw = 78, bh = 38;
    int bx = view_center_x(bw), by = view_center_y(bh);

    if (!menu_drawn) {
        view_clear();
        draw_box_shadow(bx, by, bw, bh);
    }

    /* ── Logo PAC-MAN gigante (centralizado) ── */
    int cy = by + 2;
    int logo_x = bx + (bw - 60) / 2;
    if (!menu_drawn) {
        print_abs(logo_x, cy,   CLR_TITLE,     "██████╗  █████╗  ██████╗      ███╗   ███╗ █████╗ ███╗   ██╗");
        print_abs(logo_x, cy+1, CLR_TITLE,     "██╔══██╗██╔══██╗██╔════╝      ████╗ ████║██╔══██╗████╗  ██║");
        print_abs(logo_x, cy+2, CLR_TITLE_ALT, "██████╔╝███████║██║           ██╔████╔██║███████║██╔██╗ ██║");
        print_abs(logo_x, cy+3, CLR_TITLE_ALT, "██╔═══╝ ██╔══██║██║           ██║╚██╔╝██║██╔══██║██║╚██╗██║");
        print_abs(logo_x, cy+4, CLR_TITLE,     "██║     ██║  ██║╚██████╗      ██║ ╚═╝ ██║██║  ██║██║ ╚████║");
        print_abs(logo_x, cy+5, CLR_TITLE,     "╚═╝     ╚═╝  ╚═╝ ╚═════╝      ╚═╝     ╚═╝╚═╝  ╚═╝╚═╝  ╚═══╝");
    }

    /* Sub título + HI-SCORE */
    cy += 7;
    if (!menu_drawn) {
        const char *sub = "Console Edition · Estruturas de Dados · UniCesumar";
        int slen = (int)strlen(sub) - 4; /* compensar UTF-8 do · */
        print_abs(bx + (bw - slen) / 2, cy, CLR_DIM, sub);
    }

    /* Atract animado (Pac-Man + ghosts) */
    cy += 2;
    {
        int ax = bx + 6;
        int ay = cy;
        int w = bw - 12;
        int total = w + 10;
        int t = (m->frame_count / 2) % total;

        if (t != menu_last_anim || !menu_drawn) {
            menu_last_anim = t;
            move_cursor(ax, ay);
            for (int i = 0; i < w; i++) printf(" ");

            int pellet_x = ax + ((t + w/2) % total);
            if (pellet_x < ax + w - 1 && pellet_x > ax) {
                move_cursor(pellet_x, ay);
                printf("%s ●%s", CLR_POWER, CLR_RESET);
            }

            int pac  = ax + t;
            int b    = ax + t - 3;
            int p    = ax + t - 5;
            int in   = ax + t - 7;
            int c    = ax + t - 9;

            if (c  > ax && c  < ax + w - 1) { move_cursor(c,  ay); printf("%sᗣ %s", CLR_CLYDE,  CLR_RESET); }
            if (in > ax && in < ax + w - 1) { move_cursor(in, ay); printf("%sᗣ %s", CLR_INKY,   CLR_RESET); }
            if (p  > ax && p  < ax + w - 1) { move_cursor(p,  ay); printf("%sᗣ %s", CLR_PINKY,  CLR_RESET); }
            if (b  > ax && b  < ax + w - 1) { move_cursor(b,  ay); printf("%sᗣ %s", CLR_BLINKY, CLR_RESET); }
            if (pac> ax && pac< ax + w - 1) {
                const char *sp = ((m->frame_count / 3) % 2) ? "ᗧ " : "● ";
                move_cursor(pac, ay); printf("%s%s%s", CLR_PACMAN, sp, CLR_RESET);
            }
        }
    }

    /* Divisor */
    cy += 2;
    if (!menu_drawn) draw_divider(bx, cy, bw);

    /* ── MAPA POR NÍVEL ── */
    cy += 2;
    if (!menu_drawn) print_abs(bx + 4, cy, CLR_GREEN, "▼ MAPA POR NÍVEL");
    if (!menu_drawn) print_abs(bx + 44, cy, CLR_DIM, "");

    if (!menu_drawn || m->map_selected != menu_last_map) {
        menu_last_map = m->map_selected;
        clear_rect(bx + 4, cy + 1, bw - 10, 5);
        move_cursor(bx + 4, cy + 1);
        printf("%sNível %02d   %s%s%s", CLR_TITLE, m->level, CLR_RESET,
               model_map_name(m->map_selected), CLR_RESET);
        move_cursor(bx + 4, cy + 3);
        printf("%s%s%s", CLR_SUBTITLE,
               "O próximo labirinto e escolhido automaticamente.", CLR_RESET);
    }

    /* Divisor antes do menu */
    cy += 7;
    if (!menu_drawn) draw_divider(bx, cy, bw);

    /* ── Menu de opções ── */
    cy += 2;
    if (!menu_drawn || m->menu_selected != menu_last_sel) {
        menu_last_sel = m->menu_selected;
        int mx = bx + 18;
        menu_item_styled(mx, cy + 0, m->menu_selected == 0, "Jogar  (Manual)",          "[ENTER]", 30);
        menu_item_styled(mx, cy + 1, m->menu_selected == 1, "Jogar Sozinho (Auto-Play)","[A]    ", 30);
        menu_item_styled(mx, cy + 2, m->menu_selected == 2, "Modo Explicacao",          "[E]    ", 30);
        menu_item_styled(mx, cy + 3, m->menu_selected == 3, "High Scores",              "[H]    ", 30);
        menu_item_styled(mx, cy + 4, m->menu_selected == 4, "Sair",                     "[Q]    ", 30);
    }

    cy += 7;
    {
        int blink = (m->frame_count / 12) % 2;
        if (!menu_drawn || blink != menu_last_blink) {
            menu_last_blink = blink;
            move_cursor(bx + 4, cy);
            for (int i = 0; i < bw - 8; i++) printf(" ");
            if (blink == 0) {
                const char *msg = "▲ ▼  navegar  ·  ENTER  selecionar";
                print_abs(bx + (bw - 44) / 2, cy, CLR_SUBTITLE, msg);
            }
        }
    }

    /* Rodapé arcade */
    {
        const char *foot = " INSERT COIN  ·  CREDITS 99  ·  © 2026 ";
        int flen = 38;
        move_cursor(bx + (bw - flen) / 2, by + bh - 3);
        printf("%s%s%s", CLR_DIM, foot, CLR_RESET);
    }

    view_flush();
    m->frame_count++;
    menu_drawn = 1;
}

void view_draw_skin_selector(GameModel *m) {
    view_clear();

    int W = 52, H = 16;
    int cx = view_center_x(W);
    int cy = view_center_y(H);
    draw_box_shadow(cx, cy, W, H);

    print_abs(cx + 12, cy + 2, CLR_TITLE, "★  SELECIONE SUA SKIN  ★");
    print_abs(cx + 10, cy + 3, CLR_DIM, "");

    const char *options[SKIN_COUNT] = {"Pac-Man", "Ms. Pac-Man"};
    const char *desc[SKIN_COUNT] = {
        "ᗧ",
        "ᗧ❀",
    };

    for (int i = 0; i < SKIN_COUNT; i++) {
        int y = cy + 6 + i * 2;
        move_cursor(cx + 8, y);
        if (m->skin_selected == i) {
            printf("%s▶ %s%-8s%s  %s%s%s", CLR_SEL_BG, CLR_TITLE, options[i], CLR_RESET, CLR_DIM, desc[i], CLR_RESET);
        } else {
            printf("   %s%-8s%s  %s%s%s", CLR_SUBTITLE, options[i], CLR_RESET, CLR_DIM, desc[i], CLR_RESET);
        }
    }

    move_cursor(cx + 8, cy + H - 3);
    printf("", CLR_DIM, CLR_RESET);
    view_flush();
}

void view_draw_name_entry(GameModel *m) {
    view_clear();

    int W = 54, H = 14;
    int cx = view_center_x(W);
    int cy = view_center_y(H);
    draw_box_shadow(cx, cy, W, H);

    print_abs(cx + 10, cy + 2, CLR_TITLE, "★  NOME PARA O RANKING  ★");
    print_abs(cx + 8, cy + 4, CLR_DIM, "Digite seu nome.");

    move_cursor(cx + 8, cy + 7);
    printf("%sSCORE FINAL:%s %d", CLR_SCORE, CLR_RESET, m->pending_score);

    move_cursor(cx + 8, cy + 9);
    printf("%sNOME:%s %s", CLR_TITLE, CLR_RESET, m->pending_name[0] ? m->pending_name : "(vazio)");

    move_cursor(cx + 8, cy + H - 3);
    printf("", CLR_DIM, CLR_RESET);
    view_flush();
}

void view_draw_pause_menu(GameModel *m, int selected) {
    int W = 36, H = 10;
    int cx = OFFSET_X + (MAP_W * CELL_W - W) / 2;
    int cy = OFFSET_Y + (MAP_H - H) / 2;
    /* Overlay: fill map area with a dark background so underlying map is hidden */
    for (int r = 0; r < MAP_H; r++) {
        move_cursor(OFFSET_X, OFFSET_Y + r);
        printf("%s", CLR_BG_DARK);
        for (int i = 0; i < MAP_W * CELL_W; i++) printf(" ");
        printf("%s", CLR_RESET);
    }

    draw_box_shadow(cx, cy, W, H);

    print_abs(cx + 10, cy + 2, CLR_TITLE, "── MENU DE PAUSA ──");

    const char *options[2] = {"Retomar jogo", "Sair para ranking"};
    for (int i = 0; i < 2; i++) {
        int y = cy + 4 + i * 2;
        move_cursor(cx + 6, y);
        if (selected == i) {
            printf("%s%s %s %s", CLR_SEL_BG, CLR_TITLE, options[i], CLR_RESET);
        } else {
            printf("   %s", options[i]);
        }
    }

    move_cursor(cx + 6, cy + H - 3);
    printf("", CLR_DIM, CLR_RESET);
    view_flush();
}

/* ======================== GAME OVER ======================== */

void view_draw_gameover(GameModel *m) {
    view_clear();
    
    int W = 50, H = 13;
    int cx = view_center_x(W);
    int cy = view_center_y(H);

    draw_box_shadow(cx, cy, W, H);

    int phase = 0, frames = 0;
    draw_arcade_loop(m, cx + 4, cy + 4, W - 8, &phase, &frames);

    int tx = cx + 6;
    int ty = cy + 2;
    print_abs(tx + 0, ty,   CLR_GAMEOVER, " ██████   █████  ███    ███ ███████");
    print_abs(tx + 0, ty+1, CLR_GAMEOVER, "██       ██   ██ ████  ████ ██     ");
    print_abs(tx + 0, ty+2, CLR_GAMEOVER, "██   ███ ███████ ██ ████ ██ █████  ");
    print_abs(tx + 0, ty+3, CLR_GAMEOVER, "██    ██ ██   ██ ██  ██  ██ ██     ");
    print_abs(tx + 0, ty+4, CLR_GAMEOVER, " ██████  ██   ██ ██      ██ ███████");
    print_abs(tx + 12, ty+5, CLR_GAMEOVER, "OVER");

    move_cursor(cx + 4, cy + H - 4);
    printf("%sSCORE FINAL%s    %s%6d%s", CLR_DIM, CLR_RESET, CLR_SCORE, m->score, CLR_RESET);
    move_cursor(cx + 28, cy + H - 4);
    printf("%sHI-SCORE%s    %s%6d%s", CLR_DIM, CLR_RESET, CLR_TITLE, m->high_score, CLR_RESET);

    move_cursor(cx + 4, cy + H - 3);
    printf("%sENTER%s menu  ·  %sH%s scores  ·  %sQ%s sair",
           CLR_CYAN, CLR_DIM, CLR_CYAN, CLR_DIM, CLR_CYAN, CLR_DIM);

    view_flush();
}

/* ======================== VITORIA ======================== */

void view_draw_win(GameModel *m) {
    /* Flash dramático nas paredes */
    const char *flash_colors[] = {
        CLR_TITLE, CLR_TITLE_ALT, CLR_GREEN, CLR_CYAN, CLR_MAGENTA, CLR_TITLE
    };
    for (int f = 0; f < 12; f++) {
        const char *clr = flash_colors[f % 6];
        for (int y = 0; y < MAP_H; y++)
            for (int x = 0; x < MAP_W; x++)
                if (m->grid[y][x] == TILE_WALL) {
                    const char *g = wall_glyph(m, x, y);
                    move_cursor(OFFSET_X + x * CELL_W, OFFSET_Y + y);
                    printf("%s%s%s", clr, g, CLR_RESET);
                }
        view_flush();
        sleep_ms(120);
    }

    view_clear();

    int W = 56, H = 18;
    int cx = view_center_x(W);
    int cy = view_center_y(H);
    draw_box_shadow(cx, cy, W, H);

    print_abs(cx + 12, cy + 2, CLR_WIN_CLR, "★  L A B I R I N T O   C O M P L E T O  ★");

    move_cursor(cx + 4, cy + 4);
    printf("%sSCORE FINAL%s   %s%d%s    %sNIVEL%s   %s%d%s",
           CLR_DIM, CLR_RESET, CLR_TITLE, m->score, CLR_RESET,
           CLR_DIM, CLR_RESET, CLR_CYAN, m->level, CLR_RESET);

    /* Benchmark de sorting */
    move_cursor(cx + 4, cy + 6);
    printf("%sBENCHMARK DE ORDENAÇÃO  (μs)%s", CLR_GREEN, CLR_RESET);

    const char *sort_names[7] = {"Bubble","Selection","Insertion","Shell","Merge","Quick","Heap"};

    long long max_t = 1;
    for (int i = 0; i < 7; i++)
        if (m->sort_times[i] > max_t) max_t = m->sort_times[i];

    for (int i = 0; i < 7; i++) {
        int rr = cy + 7 + i;
        move_cursor(cx + 4, rr);
        printf("%s%-10s%s ", CLR_CYAN, sort_names[i], CLR_RESET);

        int bw = 26;
        int filled = (int)((m->sort_times[i] * bw) / max_t);
        if (filled < 1 && m->sort_times[i] > 0) filled = 1;
        if (filled > bw) filled = bw;
        printf("%s", CLR_TITLE_ALT);
        for (int j = 0; j < filled; j++)       printf("█");
        printf("%s", CLR_DIM2);
        for (int j = filled; j < bw; j++)      printf("░");
        printf("%s  %s%6lld%s μs",
               CLR_RESET, CLR_SCORE, m->sort_times[i], CLR_RESET);
    }

    move_cursor(cx + 4, cy + H - 2);
    printf("%sPressione ENTER para continuar%s", CLR_DIM, CLR_RESET);

    view_flush();
}

/* ======================== HIGH SCORES (com pódio) ======================== */

void view_draw_scores(GameModel *m) {
    view_clear();

    int W = 64, H = 34;
    int cx = view_center_x(W), cy = view_center_y(H);
    draw_box_shadow(cx, cy, W, H);

    print_abs(cx + 16, cy + 2, CLR_TITLE,
              "★  H I G H   S C O R E S  ★");
    print_abs(cx + 18, cy + 3, CLR_DIM,
              "BST + Quick Sort em tempo real");

    int scores[20];
    char names[20][16];
    int count = 0;
    bst_inorder_rev(m->score_tree, scores, names, &count, 10);

    int sorted[20];
    memcpy(sorted, scores, count * sizeof(int));
    if (count > 1) quick_sort(sorted, 0, count - 1);

    /* ── Pódio para top 3 ── */
    int podium_y = cy + 5;
    int col2 = cx + 8;
    int col1 = cx + 24;
    int col3 = cx + 44;

    if (count >= 2) {
        char buf[48];
        snprintf(buf, sizeof(buf), " %-7s ", names[1]);
        print_abs(col2, podium_y,     CLR_SUBTITLE, buf);
        snprintf(buf, sizeof(buf), " %5d ", scores[1]);
        print_abs(col2, podium_y + 1, CLR_CYAN, buf);
    }
    if (count >= 1) {
        char buf[48];
        snprintf(buf, sizeof(buf), "  ★ %-5s ★", names[0]);
        print_abs(col1, podium_y - 1, CLR_TITLE, buf);
        snprintf(buf, sizeof(buf), "    %6d  ", scores[0]);
        print_abs(col1, podium_y,     CLR_TITLE, buf);
    }
    if (count >= 3) {
        char buf[48];
        snprintf(buf, sizeof(buf), " %-7s ", names[2]);
        print_abs(col3, podium_y + 2, CLR_SUBTITLE, buf);
        snprintf(buf, sizeof(buf), " %5d ", scores[2]);
        print_abs(col3, podium_y + 3, CLR_ORANGE, buf);
    }

    /* Blocos do pódio */
    if (count >= 2) {
        print_abs(col2, podium_y + 2, CLR_CYAN, " ┌─────┐ ");
        print_abs(col2, podium_y + 3, CLR_CYAN, " │  2  │ ");
        print_abs(col2, podium_y + 4, CLR_CYAN, " │     │ ");
        print_abs(col2, podium_y + 5, CLR_CYAN, " └─────┘ ");
    }
    if (count >= 1) {
        print_abs(col1, podium_y + 1, CLR_TITLE, "  ┌─────┐  ");
        print_abs(col1, podium_y + 2, CLR_TITLE, "  │  1  │  ");
        print_abs(col1, podium_y + 3, CLR_TITLE, "  │     │  ");
        print_abs(col1, podium_y + 4, CLR_TITLE, "  │     │  ");
        print_abs(col1, podium_y + 5, CLR_TITLE, "  └─────┘  ");
    }
    if (count >= 3) {
        print_abs(col3, podium_y + 4, CLR_ORANGE, " ┌─────┐ ");
        print_abs(col3, podium_y + 5, CLR_ORANGE, " └─────┘ ");
    }

    /* ── Tabela 4-10 ── */
    int ty = cy + 15;
    move_cursor(cx + 4, ty);
    printf("%s  POS   NOME           PONTOS      GRADE%s", CLR_DIM, CLR_RESET);
    move_cursor(cx + 4, ty + 1);
    printf("%s──────────────────────────────────────────%s", CLR_DIM2, CLR_RESET);

    const char *grades[] = {"S+","S","A+","A","B+","B","C+","C","D","F"};
    const char *gclr[] = {
        CLR_TITLE, CLR_TITLE, CLR_GREEN, CLR_GREEN,
        CLR_CYAN, CLR_CYAN, CLR_MAGENTA, CLR_MAGENTA,
        CLR_ORANGE, CLR_GAMEOVER
    };

    for (int i = 3; i < 10; i++) {
        move_cursor(cx + 4, ty + 2 + (i - 3));
        if (i < count) {
            printf("  %s%2d.%s   %s%-10s%s   %s%7d%s pts    %s%2s%s",
                   CLR_DIM, i + 1, CLR_RESET,
                   CLR_SUBTITLE, names[i], CLR_RESET,
                   CLR_SCORE, scores[i], CLR_RESET,
                   gclr[i], grades[i], CLR_RESET);
        } else {
            printf("  %s%2d.   ---           -------         -%s",
                   CLR_DIM2, i + 1, CLR_RESET);
        }
    }

    /* Info estruturas */
    move_cursor(cx + 4, cy + H - 5);
    printf("%sBST%s h=%-2d n=%-2d   %sAVL%s h=%-2d b=%-2d   %sSort%s QuickSort O(n log n)",
           CLR_CYAN, CLR_RESET,
           bst_height(m->score_tree), bst_count(m->score_tree),
           CLR_CYAN, CLR_RESET,
           avl_height(m->powerup_tree), avl_balance_factor(m->powerup_tree),
           CLR_CYAN, CLR_RESET);

    move_cursor(cx + 4, cy + H - 3);
    printf("%sENTER para voltar ao menu%s", CLR_DIM, CLR_RESET);

    view_flush();
}

/* ======================== MODO EXPLICAÇÃO ======================== */

void view_draw_help(GameModel *m) {
    (void)m;
    view_clear();

    int W = 68, H = 34;
    int bx = view_center_x(W), by = view_center_y(H);
    draw_box_shadow(bx, by, W, H);

    print_abs(bx + 22, by + 2, CLR_TITLE, "★  M O D O   E X P L I C A C A O  ★");
    print_abs(bx + 18, by + 3, CLR_DIM,   "Como ganhar + estruturas por trás do jogo");

    int r = by + 5;

    move_cursor(bx + 3, r++); printf("%s● OBJETIVO%s", CLR_GREEN, CLR_RESET);
    move_cursor(bx + 5, r++); printf("%sColete todos os pontos do labirinto sem ser pego.%s", CLR_SUBTITLE, CLR_RESET);
    move_cursor(bx + 5, r++); printf("%sZerou os pontos → próximo nível.%s", CLR_SUBTITLE, CLR_RESET);
    r++;

    move_cursor(bx + 3, r++); printf("%s● POWER PELLET  %s ●  %s(50 pts)%s",
                                     CLR_GREEN, CLR_POWER, CLR_DIM, CLR_RESET);
    move_cursor(bx + 5, r++); printf("%sFantasmas viram azuis e podem ser comidos por:%s", CLR_SUBTITLE, CLR_RESET);
    move_cursor(bx + 5, r++); printf("  %s200 → 400 → 800 → 1600 pts%s (combo dobra a cada um)",
                                     CLR_ORANGE, CLR_RESET);
    r++;

    move_cursor(bx + 3, r++); printf("%s● FANTASMAS E SUAS IA%s", CLR_GREEN, CLR_RESET);
    move_cursor(bx + 5, r++); printf("%sᗣ%s %sBlinky%s vermelho: %sDijkstra%s (caminho mínimo)",
                                     CLR_BLINKY, CLR_RESET, CLR_BLINKY, CLR_RESET, CLR_CYAN, CLR_RESET);
    move_cursor(bx + 5, r++); printf("%sᗣ%s %sPinky%s  rosa:     %sBFS%s (largura, antecipa Pac-Man)",
                                     CLR_PINKY, CLR_RESET, CLR_PINKY, CLR_RESET, CLR_CYAN, CLR_RESET);
    move_cursor(bx + 5, r++); printf("%sᗣ%s %sInky%s   ciano:    %sDFS%s (profundidade, imprevisível)",
                                     CLR_INKY, CLR_RESET, CLR_INKY, CLR_RESET, CLR_CYAN, CLR_RESET);
    move_cursor(bx + 5, r++); printf("%sᗣ%s %sClyde%s  laranja:  %sManhattan%s (distância simples)",
                                     CLR_CLYDE, CLR_RESET, CLR_CLYDE, CLR_RESET, CLR_CYAN, CLR_RESET);
    r++;

    move_cursor(bx + 3, r++); printf("%s● CONTROLES%s", CLR_GREEN, CLR_RESET);
    move_cursor(bx + 5, r++); printf("%sW A S D%s ou %s← ↑ ↓ →%s   mover Pac-Man",
                                     CLR_CYAN, CLR_RESET, CLR_CYAN, CLR_RESET);
    move_cursor(bx + 5, r++); printf("%sP%s pausa    %sQ%s sair    %sH%s scores",
                                     CLR_CYAN, CLR_RESET, CLR_CYAN, CLR_RESET, CLR_CYAN, CLR_RESET);
    r++;

    move_cursor(bx + 3, r++); printf("%s● AUTO-PLAY%s", CLR_GREEN, CLR_RESET);
    move_cursor(bx + 5, r++); printf("%sSimulated Annealing planeja vários passos à frente.%s",
                                     CLR_SUBTITLE, CLR_RESET);
    move_cursor(bx + 5, r++); printf("%sNo power, muda para HUNT e prioriza fantasmas vulneráveis.%s",
                                     CLR_SUBTITLE, CLR_RESET);

    move_cursor(bx + 3, by + H - 3);
    printf("%sENTER / Esc — voltar ao menu%s", CLR_DIM, CLR_RESET);

    view_flush();
}
