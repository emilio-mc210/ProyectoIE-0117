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
