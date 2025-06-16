#ifndef CHARDEV_H
#define CHARDEV_H

int init_chardev(void);

void cleanup_chardev(void);

//ssize_t es un tipo de dato para representar un numero de bytes o indicar un error si es negativo

//Funcion para leer el buffer
static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset);

//Funcion para escribir en el buffer
static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset);

#endif