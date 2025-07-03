# Proyecto IE-0117
Este proyecto implementa un módulo del kernel de Linux que registra un dispositivo de caracteres (char device) y un programa en espacio de usuario que interactúa con el dispositivo mediante operaciones estándar como lectura y escritura.

## Dependencias y pasos para instalación

### Biblioteca `vrg`
Se debe descargar el archivo `vrg.h` de https://github.com/rdentato/vrg y colocarse en el directorio `src/` para poder compilar, no se puede instalar por medio de un administrador de paquetes.

### Requisitos

- Sistema operativo Linux
- Compilador GCC
- Make (construcción del proyecto)
- Permisos de super usuario (`sudo`)  
- Paquetes necesarios para compilar módulos del kernel:
```bash
sudo apt-get install build-esential kmod
sudo apt-get install linux-headers-`uname -r`
```

**Recomendación**: Utilizar una máquina virtual para evitar que un error inesperado dañe el sistema operativo del usuario.

### Compilación

El usuario se debe posicionar en el directorio donde clonó el dispositivo y construir el proyecto con `make`.
```bash
cd ~/usr/ProyectoIE-0117/
make
```
## Ejecución del código
Una vez compilado, lo primero que se debe hacer es insertar el módulo al sistema. Para esto se debe usar `insmod` y se tiene que colocar el archivo `modulo.ko`, cualquier otro archivo generado con `make` no es el módulo de kernel.
```bash
sudo insmod modulo.ko
```
Posteriormente, para poder utilizar el programa se le debe dar permisos de escritura y lectura al dispositivo de caracteres creado por el módulo, que se puede lograr con `chmod`.
```bash
sudo chmod 666 /dev/chardev
```
Finalmente, se puede utilizar el programa `cli` para poder interactuar con el módulo, ingresar `./cli <texto>` va a escribir el texto digitado al char device, pero también se puede hacer uso de flags como `-r`, `-l`, `--clean` para leer, obtener la última entrada o limpiar el dispositivo rspectivamente. Para más información se puede utilizar `-h` o visitar la [Wiki](https://github.com/emilio-mc210/ProyectoIE-0117/wiki).

Es **importante** que cuando se termina de utilizar el programa es necesario desmontar el módulo de kernel, así se evitan comportamientos inesperados por parte del sistema operativo. Esto se realiza con el comando `rmmod`.
```bash
sudo rmmod modulo
```
    
### Otros comandos utiles para el proyecto

- `sudo dmesg -W` permite ver los mensajes que se hacen en con printk().
- `cat /proc/devices | grep chardev` sirve para revisar si se creó el char device exitosamente.
- `cat /proc/modules | grep modulo` sirve para comprobar si se montó el módulo de kernel con éxito.
- `lsmod` imprime los módulos cargados en el kernel. 
