#include<linux/fs.h>
#include<linux/uaccess.h>
#include<linux/cdev.h>
//Incluir funcionalidades
//ej: #include"count.h"
#include"last.h"
#include"chardev.h"

#define DEVICE_NAME "chardev" //Nombre como aparece en /proc/devices
#define ENTRY_SIZE 128
#define MAX_ENTRIES 10

//Variables globales para este archivo
static int major; //Numero mayor asignado al char device
static struct class *char_class = NULL;
static struct device *char_device = NULL;

//Arreglo de punteros, donde cada uno es un bloque de tamano ENTRY_SIZE
static char *buffer[MAX_ENTRIES];

//head: índice donde se va a almacenar una entrada nueva.
//tail: índice de la entrada más vieja.
//count: cuántas entradas válidas hay actualmente en el buffer.
static int head = 0, tail = 0, count = 0;

//Operaciones del device
static struct file_operations fops = {
    .read = dev_read,
    .write = dev_write
};

static int init_chardev(void) {
    //Obtencion del numero mayor para el char device
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ALERT "Registro del char device fallo con %i\n", major);
        return major;  //No encontro un numero valido
    }

    //Exito en obtener el numero mayor
    printk(KERN_INFO "Registro de char device exitoso con numero mayor %i\n", major);

    char_class = class_create(DEVICE_NAME);

    char_device = device_create(char_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

    printk(KERN_INFO "Char device creado en /dev/%s\n", DEVICE_NAME);

    return 0;
}

void cleanup_chardev(void) {
    for (int i = 0; i < MAX_ENTRIES; i++) {
        kfree(buffer[i]);
    }

    //Quitar del registro
    device_destroy(char_class, MKDEV(major, 0));
    class_destroy(char_class);
    unregister_chrdev(major, DEVICE_NAME);
}

static ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    const char *msg = get_last_message();
    size_t msg_len = strlen(msg);
    size_t to_copy = min(len, msg_len - *offset);

    if (*offset >= msg_len) return 0;

    if (copy_to_user(buffer, msg + *offset, to_copy) != 0)
        return -EFAULT;

    *offset += to_copy;

    return to_copy;
}

static ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char *kbuf = kmalloc(len + 1, GFP_KERNEL);

    if (!kbuf) return -ENOMEM;

    if (copy_from_user(kbuf, buffer, len) != 0) {
        kfree(kbuf);
        return -EFAULT;
    }

    kbuf[len] = '\0';

    set_last_message(kbuf);

    increment_count();

    kfree(kbuf);

    return len;
}