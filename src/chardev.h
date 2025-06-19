#ifndef CHARDEV_H
#define CHARDEV_H
#define DEVICE_NAME "chardev" //Nombre del char device como aparece en /proc/devices
#define ENTRY_SIZE 128 //Tamaño máximo en bytes que puede tener cada entrada en el buffer 
#define MAX_ENTRIES 10 //Capacidad máxima de entradas que puede almacenar el buffer

int init_chardev(void);
void cleanup_chardev(void);

//ssize_t es un tipo de dato para representar un numero de bytes o indicar un error si es negativo

//Funcion para leer el buffer
ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset);

//Funcion para escribir en el buffer, las funciones no deben ser static porque se usan en otros archivos
ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset);

//Funcion para abrir el dispositivo
int dev_open(struct inode *inode, struct file *filep);
//Funcion para cerrar el dispositivo/archivo
int dev_release(struct inode *inode, struct file *filep);

#endif
