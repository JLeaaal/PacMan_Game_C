# PAC-MAN Console Edition

Este é um jogo PAC-MAN implementado em C, utilizando estruturas de dados avançadas para demonstrar conceitos acadêmicos.

## Funcionalidades Implementadas

### Estruturas de Dados
- **Árvore Binária de Busca (BST)**: Utilizada para armazenar e gerenciar os high scores.
- **Árvore AVL (Balanceada)**: Utilizada para o inventário de power-ups.
- **Grafos**: Representação do labirinto com matriz de adjacência e lista de adjacência.
- **Busca em Largura (BFS)**: IA do fantasma Pinky e modo auto-play do Pac-Man.
- **Busca em Profundidade (DFS)**: IA do fantasma Inky.
- **Algoritmo de Dijkstra**: IA do fantasma Blinky.
- **Ordenação**: Bubble, Selection, Insertion, Shell, Merge, Quick, Heap Sort para rankings.

### Jogabilidade
- Labirinto clássico de 28x31.
- Pac-Man controlável com WASD ou setas.
- 4 fantasmas com IA inteligente.
- Pontuação, vidas (3 iniciais), níveis.
- Power pellets para tornar fantasmas vulneráveis.
- Modo auto-play opcional.
- Rankings persistentes.

### Interface
- Interface gráfica baseada em ANSI escape codes (compatível com Windows Terminal).
- Menu principal, HUD, telas de pausa, game over, vitória.
- Painel lateral com informações em tempo real das estruturas de dados.

## Como Compilar e Executar

### Linux/macOS
```bash
make
./pacman
```

### Windows
```bash
mingw32-make
pacman.exe
```

## Controles
- **WASD** ou **Setas**: Mover Pac-Man
- **P**: Pausar/Despausar
- **Q**: Sair

## Modo Auto-Play
Execute com variável de ambiente:
```bash
PACMAN_AUTO=1 ./pacman
```

## Estruturas de Dados em Detalhes

### BST para High Scores
- Inserção, busca, remoção balanceada.
- Ordenação in-order para rankings.

### AVL para Power-Ups
- Inserção balanceada para manter altura log n.
- Inventário de itens coletáveis.

### Grafo do Labirinto
- Nós representam posições (x,y).
- Arestas para movimentos possíveis.
- Algoritmos de busca para IA.

## Requisitos do Sistema
- Compilador C (GCC recomendado).
- Biblioteca ncurses (planejada).
- Terminal com suporte a cores ANSI.
