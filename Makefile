# Define los archivos fuente del módulo (con el path relativo)
obj-m += modulo.o

# Archivos adicionales que componen el módulo
modulo-objs := src/modulo.o src/chardev.o src/last.o

# Ruta al directorio de construcción del kernel
KDIR := /lib/modules/$(shell uname -r)/build

# Ruta al directorio actual
PWD := $(shell pwd)

# Nombre del programa CLI
CLI_SRC = src/cli.c
CLI_BIN = cli

# Regla por defecto para compilar el modulo y el CLI
all: modulo cli

modulo: 
	make -C $(KDIR) M=$(PWD) modules

cli:
	gcc -Wall -o $(CLI_BIN) $(CLI_SRC)

# Limpiar archivos generados
clean:
	make -C $(KDIR) M=$(PWD) clean 
	rm -f $(CLI_BIN)



