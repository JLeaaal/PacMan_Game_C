# ============================================================
#  PAC-MAN MVC — Makefile
# ============================================================
#

#  Compilar:
#    Linux/macOS:  make
#    Windows:      mingw32-make  (ou use o comando manual abaixo)
#
#  Comando manual (funciona em qualquer plataforma):
#    gcc -I. -o pacman controller/controller.c model/model.c view/view.c -lm
#
#  Limpar:
#    make clean
# ============================================================

CC      = gcc
CFLAGS  = -Wall -Wextra -I.
LDFLAGS = -lm
TARGET  = pacman
SRC     = controller/controller.c model/model.c view/view.c
OBJ     = $(SRC:.c=.o)

# Detecta Windows
ifeq ($(OS),Windows_NT)
    TARGET := $(TARGET).exe
    RM = del /Q
    LDFLAGS += -lwinmm
else
    RM = rm -f
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

clean:
	$(RM) $(TARGET)
	$(RM) model/*.o view/*.o controller/*.o

# Sugestões para melhorias (não implementadas):
# - run: ./pacman
# - debug: gcc -I. -g -o pacman controller/controller.c model/model.c view/view.c -lm
# - Regras de objeto separadas para recompilação incremental

.PHONY: all clean
