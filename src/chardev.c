#include<linux/fs.h> //define estructuras y funciones para manejar archivos dentro del kernel 
#include<linux/uaccess.h> //funciones para copiar datos entre el user space a espacio de kernel 
#include<linux/cdev.h> //manejo de char devices
//Incluir funcionalidades
//ej: #include"count.h"
#include"last.h"
#include"chardev.h"

#define DEVICE_NAME "chardev" //Nombre como aparece en /proc/devices
#define ENTRY_SIZE 128 //futura implememtacion del buffer circular
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
//tatic int head = 0, tail = 0, count = 0;

//Operaciones del device
static struct file_operations fops = {
    .read = dev_read,
    .write = dev_write
};

int init_chardev(void) {
    //Obtencion del numero mayor para el char device
    major = register_chrdev(0, DEVICE_NAME, &fops);
    if (major < 0) {
        printk(KERN_ALERT "Registro del char device fallo con %i\n", major);
        return major;  //No encontro un numero valido
    }
    //Exito en obtener el numero mayor
    printk(KERN_INFO "Registro de char device exitoso con numero mayor %i\n", major);

    char_class = class_create(DEVICE_NAME); //crea una clase y un dispositivo en /dev/chardev
        if (IS_ERR(char_class)) {
        unregister_chrdev(major, DEVICE_NAME);
        return PTR_ERR(char_class);
    }

    char_device = device_create(char_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);
    if (IS_ERR(char_device)) {
        class_destroy(char_class);
        unregister_chrdev(major, DEVICE_NAME);
        return PTR_ERR(char_device);
    }


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

ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    const char *msg = get_last_message(); //obtiene el ultimo mensaje guardado en el kernel 
    size_t msg_len = strlen(msg); //longitud real del mensaje
    size_t to_copy = min(len, msg_len - *offset); //se decide cuantos bytes copiar al usuario 

    if (*offset >= msg_len) return 0; //verifica si ya se leyo todo el mensaje

    if (copy_to_user(buffer, msg + *offset, to_copy) != 0) //
        return -EFAULT;

    *offset += to_copy;
    return to_copy;
}

ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    char *kbuf = kmalloc(len + 1, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    if (copy_from_user(kbuf, buffer, len) != 0) {
        kfree(kbuf);
        return -EFAULT;
    }

    kbuf[len] = '\0';
    set_last_message(kbuf);
    //increment_count();
    kfree(kbuf);

    return len;
}