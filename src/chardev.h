/*Header CHARDEV_H para el driver de dispositivo de caracteres
*Se definen constantes de configuración y funciones
*DEVICE_NAME: define nombre del dispositivo que aparecerá en /dev y /proc/devices
*ENTRY_SIZE: define el tamaño máximo en bytes para cada entrada del buffer 
*MAX_ENTRIES: define la capacidad máxima de mensjaes en el buffer circular
 */
#ifndef CHARDEV_H
#define CHARDEV_H
#define DEVICE_NAME "chardev" 
#define ENTRY_SIZE 128 
#define MAX_ENTRIES 10

//Función para inicializar y registrar el dispositivo 
int init_chardev(void);

//Función para liberar todos los recursos del dispositivo
void cleanup_chardev(void);

//Función para limpiar todas las entradas del dispositivo
void clear_chardev(void); 

//Funcion para leer el dispositivo
ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset);

//Funcion para escribir en el dispositivo
ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset);

//Funcion para abrir el dispositivo
int dev_open(struct inode *inode, struct file *filep);

//Funcion para cerrar el dispositivo/archivo
int dev_release(struct inode *inode, struct file *filep);

#endif
