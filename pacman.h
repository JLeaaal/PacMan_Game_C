/*
 * ============================================================
 *  PAC-MAN Console — Projeto MVC em C  [VERSAO APRIMORADA]
 * ============================================================
 *  Estruturas de dados academicas integradas ao gameplay:
 *
 *  - Arvore Binaria de Busca (BST)  → ranking de high scores
 *  - Arvore AVL (balanceada)        → inventario de power-ups
 *  - Grafos (matriz + lista adj.)   → representacao do labirinto
 *  - Busca em profundidade (DFS)    → IA do fantasma Inky
 *  - Busca em largura (BFS)         → IA do fantasma Pinky + Auto-Play
 *  - Dijkstra                       → IA do fantasma Blinky
 *  - Bubble/Selection/Insertion/Shell/Merge/Quick/Heap Sort → rankings
 * ============================================================
 */

#ifndef PACMAN_H
#define PACMAN_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <conio.h>
#include <windows.h>
#ifndef ENABLE_VIRTUAL_TERMINAL_PROCESSING
#define ENABLE_VIRTUAL_TERMINAL_PROCESSING 0x0004
#endif
#else
#include <unistd.h>
#include <termios.h>
#include <fcntl.h>
#endif

/* ======================== CONSTANTES ======================== */

#define MAP_W       28
#define MAP_H       31
#define GHOSTS      4
#define FRAME_MS    85
#define POWER_TIME  120
#define START_LIVES 4
#define MAX_SCORES  100
#define CELL_W      2
int view_offset_x(void);
int view_offset_y(void);
#define OFFSET_X view_offset_x()
#define OFFSET_Y view_offset_y()
#define TUNNEL_ROW  14
#define INF         999999
#define MAP_COUNT   3
#define SCORE_FILE  "highscores.txt"
#define MAX_NAME_LEN 15
#define SKIN_COUNT  2

/* ======================== PALETA DE CORES 256 (estilo arcade clássico) ======================== */

/* Paredes: azul-eléctrico arcade (estilo gabinete original do Pac-Man) */
#define CLR_WALL        "\033[38;5;33m"
#define CLR_WALL_DIM    "\033[38;5;19m"
#define CLR_WALL_HI     "\033[38;5;39m"
#define CLR_PELLET      "\033[38;5;223m"
#define CLR_POWER       "\033[38;5;221;1m"
#define CLR_DOOR        "\033[38;5;217m"

#define CLR_PACMAN      "\033[38;5;226;1m"
#define CLR_PACMAN_SHADOW "\033[38;5;220m"
#define CLR_BLINKY      "\033[38;5;196;1m"
#define CLR_PINKY       "\033[38;5;213;1m"
#define CLR_INKY        "\033[38;5;51;1m"
#define CLR_CLYDE       "\033[38;5;214;1m"
#define CLR_VULN        "\033[38;5;27;1m"
#define CLR_VULN2       "\033[38;5;231;1m"

#define CLR_TITLE       "\033[38;5;226;1m"
#define CLR_TITLE_ALT   "\033[38;5;208;1m"
#define CLR_SUBTITLE    "\033[38;5;229m"
#define CLR_SCORE       "\033[38;5;231;1m"
#define CLR_GREEN       "\033[38;5;118;1m"
#define CLR_CYAN        "\033[38;5;51m"
#define CLR_MAGENTA     "\033[38;5;213m"
#define CLR_ORANGE      "\033[38;5;214m"
#define CLR_RED         "\033[38;5;196;1m"
#define CLR_DIM         "\033[38;5;243m"
#define CLR_DIM2        "\033[38;5;238m"
#define CLR_PANEL_BORDER "\033[38;5;240m"
#define CLR_PANEL_BORDER2 "\033[38;5;238m"
#define CLR_PANEL_HDR   "\033[38;5;75m"

#define CLR_READY       "\033[38;5;226;1m"
#define CLR_GAMEOVER    "\033[38;5;196;1m"
#define CLR_WIN_CLR     "\033[38;5;118;1m"
#define CLR_AUTO_TAG    "\033[38;5;118;1m"

#define CLR_HL_BG       "\033[48;5;234m"
#define CLR_SEL_BG      "\033[48;5;236m"
#define CLR_BG_DARK     "\033[48;5;232m"

#define CLR_RESET       "\033[0m"

/* ======================== ENUMS ======================== */

typedef enum {
    TILE_EMPTY = 0, TILE_WALL, TILE_PELLET, TILE_POWER, TILE_DOOR, TILE_TUNNEL
} TileType;

typedef enum {
    STATE_MENU, STATE_SKIN_SELECT, STATE_PLAYING, STATE_PAUSED, STATE_DYING,
    STATE_WIN, STATE_GAMEOVER, STATE_NAME_ENTRY, STATE_SCORES, STATE_HELP, STATE_QUIT
} GameState;

/* ======================== ENTIDADES ======================== */

typedef struct { int x, y, dx, dy, start_x, start_y; } Entity;

typedef struct {
    Entity e;
    char symbol;
    int vulnerable, eaten, in_house, release_timer;
} Ghost;

/* ======================== ESTRUTURAS DE DADOS ======================== */

typedef struct BSTNode {
    int score; char name[16];
    struct BSTNode *left, *right;
} BSTNode;

typedef struct AVLNode {
    int key, value, height;
    struct AVLNode *left, *right;
} AVLNode;

typedef struct AdjNode {
    int vertex, weight;
    struct AdjNode *next;
} AdjNode;

typedef struct {
    int num_vertices;
    AdjNode **adj_list;
    int *adj_matrix;
    int matrix_allocated;
} Graph;

typedef struct QueueNode { int data; struct QueueNode *next; } QueueNode;
typedef struct { QueueNode *front, *rear; int size; } Queue;
typedef struct { int vertex, dist; } HeapItem;
typedef struct { HeapItem *items; int size, capacity; } MinHeap;

/* ======================== GAME MODEL ======================== */

typedef struct {
    int grid[MAP_H][MAP_W];
    int pellets_left, total_pellets;

    Entity pacman;
    Ghost  ghosts[GHOSTS];

    GameState state;
    int score, high_score, lives, level;
    int power_timer, ghost_eat_combo;
    int frame_count, ready_timer, death_anim;
    int frame_ms;
    int running;

    int desired_dx, desired_dy;
    int current_dx, current_dy;

    BSTNode *score_tree;
    AVLNode *powerup_tree;
    Graph   *maze_graph;

    int dijkstra_dist[MAP_H * MAP_W];
    int dijkstra_prev[MAP_H * MAP_W];
    int bfs_dist[MAP_H * MAP_W];
    int bfs_prev[MAP_H * MAP_W];
    int dfs_path[MAP_H * MAP_W];
    int dfs_path_len;

    int score_history[MAX_SCORES];
    int score_history_count;

    /* Modo auto-play */
    int auto_play;
    int auto_update_counter;
    char auto_status[64];
    int auto_pellets_eaten;
    int auto_ghosts_eaten;
    int phase_ghosts_eaten;
    int phase_fruits_eaten;
    int phase_start_ticks;
    int cumulative_ghosts_eaten;
    int cumulative_fruits_eaten;
    int cumulative_ghosts_eaten_by_ghost[GHOSTS];

    /* Overlay de bônus (ex.: +200 ao comer fantasma) */
    int bonus_timer;   /* frames restantes */
    int bonus_points;
    int bonus_x, bonus_y; /* coordenadas no grid */

    /* Tempos de sorting (em microsegundos) */
    long long sort_times[7]; /* bubble, selection, insertion, shell, merge, quick, heap */

    /* Buffer para algoritmos de busca */
    int search_visited[MAP_H * MAP_W];

    /* Menu */
    int menu_selected; /* 0..N-1 */
    int map_selected;  /* 0..MAP_COUNT-1 */
    int skin_selected; /* 0..SKIN_COUNT-1 */

    int pending_score;
    char pending_name[MAX_NAME_LEN + 1];
    int pending_name_len;

    /* Splash mostrado uma vez */
    int splash_shown;

} GameModel;

/* ======================== PROTOTIPOS — MODEL ======================== */

BSTNode *bst_insert(BSTNode *root, int score, const char *name);
BSTNode *bst_search(BSTNode *root, int score);

int      model_load_scores(GameModel *m);
int      model_save_scores(GameModel *m);
int      model_score_qualifies(GameModel *m, int score);
const char *model_skin_name(int index);
const char *model_skin_color(int index);
BSTNode *bst_remove(BSTNode *root, int score);
void     bst_inorder(BSTNode *root, int *arr, int *count, int max);
void     bst_inorder_rev(BSTNode *root, int *arr, char names[][16], int *count, int max);
BSTNode *bst_min(BSTNode *root);
BSTNode *bst_max(BSTNode *root);
int      bst_height(BSTNode *root);
int      bst_count(BSTNode *root);
void     bst_free(BSTNode *root);

AVLNode *avl_insert(AVLNode *root, int key, int value);
AVLNode *avl_remove(AVLNode *root, int key);
AVLNode *avl_search(AVLNode *root, int key);
int      avl_height(AVLNode *node);
AVLNode *avl_rotate_left(AVLNode *node);
AVLNode *avl_rotate_right(AVLNode *node);
AVLNode *avl_rotate_left_right(AVLNode *node);
AVLNode *avl_rotate_right_left(AVLNode *node);
int      avl_balance_factor(AVLNode *node);
void     avl_free(AVLNode *root);

Graph   *graph_create(int num_vertices);
void     graph_add_edge(Graph *g, int u, int v, int weight);
void     graph_build_from_map(Graph *g, int grid[MAP_H][MAP_W]);
void     graph_free(Graph *g);
int      graph_has_edge_matrix(Graph *g, int u, int v);
int      graph_has_edge_list(Graph *g, int u, int v);

void     bfs(Graph *g, int src, int *dist, int *prev);
void     dfs(Graph *g, int src, int dst, int *visited, int *path, int *path_len);
void     dijkstra(Graph *g, int src, int *dist, int *prev, int *visited);

void     bubble_sort(int *arr, int n);
void     selection_sort(int *arr, int n);
void     insertion_sort(int *arr, int n);
void     shell_sort(int *arr, int n);
void     merge_sort(int *arr, int n);
void     quick_sort(int *arr, int low, int high);
void     heap_sort(int *arr, int n);

MinHeap *heap_create(int capacity);
void     heap_push(MinHeap *h, int vertex, int dist);
HeapItem heap_pop(MinHeap *h);
int      heap_empty(MinHeap *h);
void     heap_free(MinHeap *h);

Queue   *queue_create(void);
void     queue_push(Queue *q, int data);
int      queue_pop(Queue *q);
int      queue_empty(Queue *q);
void     queue_free(Queue *q);

void     model_init(GameModel *m);
void     model_load_map(GameModel *m);
const char *model_map_name(int index);
void     model_reset_positions(GameModel *m);
void     model_move_pacman(GameModel *m);
void     model_move_ghosts(GameModel *m);
void     view_draw_skin_selector(GameModel *m);
void     view_draw_name_entry(GameModel *m);
void     view_draw_pause_menu(GameModel *m, int selected);
int      model_check_collisions(GameModel *m);
int      model_is_walkable(GameModel *m, int x, int y, int for_ghost);
void     model_free(GameModel *m);
int      model_coord_to_vertex(int x, int y);
void     model_vertex_to_coord(int v, int *x, int *y);

/* ======================== PROTOTIPOS — VIEW ======================== */

void  view_init(void);
void  view_cleanup(void);
void  view_clear(void);
void  view_draw_splash(GameModel *m);
void  view_draw_menu(GameModel *m);
void  view_reset_menu_state(void);
void  view_draw_game(GameModel *m);
void  view_draw_map(GameModel *m);
void  view_draw_entities(GameModel *m);
void  view_erase_entities(GameModel *m);
void  view_draw_hud(GameModel *m);
void  view_draw_side_panel(GameModel *m);
void  view_draw_ready(void);
void  view_clear_ready(GameModel *m);
void  view_draw_gameover(GameModel *m);
void  view_draw_win(GameModel *m);
void  view_draw_death_anim(GameModel *m, int frame);
void  view_draw_scores(GameModel *m);
void  view_draw_help(GameModel *m);
void  view_draw_cell(GameModel *m, int x, int y);
void  view_draw_bonus(GameModel *m);
void  view_restore_bonus_cell(GameModel *m, int x, int y);
void  view_flush(void);

/* ======================== PROTOTIPOS — CONTROLLER ======================== */

void  controller_run(GameModel *m);
void  controller_handle_input(GameModel *m);
void  controller_update(GameModel *m);
void  controller_menu_input(GameModel *m);
void  controller_auto_play(GameModel *m);

/* ======================== UTILIDADES ======================== */

void  sleep_ms(int ms);
void  enable_ansi(void);
int   kbhit_custom(void);
int   getch_custom(void);
int   manhattan(int x1, int y1, int x2, int y2);

#endif /* PACMAN_H */
