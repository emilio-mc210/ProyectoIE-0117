# ProyectoIE-0117
Proyecto de Plataformas

## Comandos utiles para el proyecto

1. `sudo chmod 666 /dev/chardev` permite uso del char device con mayor facilidad.
2. `sudo insmod modulo.ko` monta el modulo en el sistema, iniciandolo y por ende creando el char device.
3. `sudo dmesg -W` permite ver los mensajes que se hacen en "KERN_INFO" con printk().
4. `sudo rmmod modulo` desmonta el modulo del sistema. 
5. `cat /proc/devices | grep chardev` sirve para revisar si se creo el char device exitosamente.
6. `cat /proc/modules | grep modulo` sirve para comprobar si se monto el modulo de kernel con exito.
7. `lsmod` imprime los modulos cargados en el kernel. 


## Flags para el programa userspace `cli_parser`

* -h Muestra ayuda de como utilizar el programa.
* -l Muestra el ultimo mensaje del char device.
* -w Escribe en el char device.
* -r Lee el char device, imprime todas las entradas.
* -co Cuenta la cantidad de entradas en el char device (de count).
* -cl Limpia las entradas del char device (de clear). 

##Makefile
1. `make` compila todo
2. `make cli` compila el CLI
3. `make modulo` compila solo el modulo
