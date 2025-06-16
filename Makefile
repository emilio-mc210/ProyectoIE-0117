# Define los archivos fuente del módulo (con el path relativo)
obj-m += modulo.o

# Archivos adicionales que componen el módulo
modulo-objs := src/modulo.o src/chardev.o src/last.o

# Ruta al directorio de construcción del kernel
KDIR := /lib/modules/$(shell uname -r)/build

# Ruta al directorio actual
PWD := $(shell pwd)

# Regla por defecto: compilar el módulo
all:
	make -C $(KDIR) M=$(PWD) modules

# Limpiar archivos generados
clean:
	make -C $(KDIR) M=$(PWD) clean

