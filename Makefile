# Nombre del programa final
EXEC = programa

# Compilador a usar
CC = gcc

# Banderas de compilación (-Iinclude es para que encuentre los .h)
CFLAGS = -Iinclude $(shell pkg-config --cflags gtk+-3.0 vte-2.91)

# Librerías de GTK y VTE
LIBS = $(shell pkg-config --libs gtk+-3.0 vte-2.91)

# Lista de todos los archivos .c que están en la carpeta src/
SRCS = src/main.c src/bienvenida.c src/tareas.c src/archivos.c src/consola.c src/gestor_archivos.c src/gestor_respaldos.c src/bash.c src/descargas.c

# Regla principal para compilar
all:
	$(CC) $(SRCS) -o $(EXEC) $(CFLAGS) $(LIBS)

# Regla para limpiar
clean:
	rm -f $(EXEC)

# Ejecutar
run:
	./programa