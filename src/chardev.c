#include<linux/fs.h> //define estructuras y funciones para manejar archivos dentro del kernel 
#include<linux/uaccess.h> //funciones para copiar datos entre el user space a espacio de kernel 
#include<linux/cdev.h> //manejo de char devices
//Incluir funcionalidades
#include <linux/slab.h> //permite asignacion de memoria dentro del kernel
//ej: #include"count.h"
#include"last.h"
#include <linux/spinlock.h>
#include"chardev.h"

#define DEVICE_NAME "chardev" //Nombre como aparece en /proc/devices
#define ENTRY_SIZE 128 //futura implememtacion del buffer circular
#define MAX_ENTRIES 10

//Variables globales para este archivo
static int major; //Numero mayor asignado al char device
static struct class *char_class = NULL;
static struct device *char_device = NULL;

//Arreglo de punteros, donde cada uno es un bloque de tamano ENTRY_SIZE
//static char *buffer[MAX_ENTRIES];
static struct {
    char *entradas[MAX_ENTRIES]; //array de punteros a strings.
    int head;
    int tail;
    int count;
    spinlock_t lock; //permite sincronizar acceso al buffer 
} circ_buffer;

//head: índice donde se va a almacenar una entrada nueva.
//tail: índice de la entrada más vieja.
//count: cuántas entradas válidas hay actualmente en el buffer.


//Operaciones del device
static struct file_operations fops = {
    .read = dev_read,
    .write = dev_write
};

int init_chardev(void) {
    //inicializar el buffer circular
    int i;
    spin_lock_init(&circ_buffer.lock); //inicializa el spinlock para proteger el buffer
    circ_buffer.head = 0;
    circ_buffer.tail = 0;
    circ_buffer.count = 0;
    for (i = 0; i < MAX_ENTRIES; i++) {
        circ_buffer.entries[i] = NULL;
    }
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
    unsigned long flags;
    spin_lock_irqsave(&circ_buffer.lock, flags);
    for (int i = 0; i < MAX_ENTRIES; i++) {
        kfree(circ_buffer.entradas[i]);
        circ_buffer.entries[i] = NULL;
    }
    spin_unlock_irqrestore(&circ_buffer.lock, flags);

    //Quitar del registro
    device_destroy(char_class, MKDEV(major, 0));
    class_destroy(char_class);
    unregister_chrdev(major, DEVICE_NAME);
}

ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    
    unsigned long flags;
    char *msg = NULL;
    int ret = 0;
    spin_lock_irqsave(&circ_buffer.lock, flags);
    if (circ_buffer.count == 0) {
        spin_unlock_irqrestore(&circ_buffer.lock, flags);
        return 0; // No hay mensajes para leer
    }
    msg = circ_buffer.entries[circ_buffer.tail];
    spin_unlock_irqrestore(&circ_buffer.lock, flags);
    if (!msg) return 0;

    size_t msg_len = strlen(msg); //longitud real del mensaje
    size_t to_copy = min(len, msg_len - *offset); //se decide cuantos bytes copiar al usuario 

    if (*offset >= msg_len) {
        spin_lock_irqsave(&circ_buffer.lock, flags);
        circ_buffer.tail = (circ_buffer.tail + 1) % MAX_ENTRIES;
        circ_buffer.count--;
        spin_unlock_irqrestore(&circ_buffer.lock, flags);
        *offset = 0;
        
        return 0; //verifica si ya se leyo todo el mensaje
    }
    if (copy_to_user(buffer, msg + *offset, to_copy) != 0) //
        return -EFAULT;

    *offset += to_copy;
    return to_copy;
}

ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    unsigned long flags;
    char *kbuf = kmalloc(len + 1, GFP_KERNEL);
    if (!kbuf) return -ENOMEM;

    if (copy_from_user(kbuf, buffer, len) != 0) {
        kfree(kbuf);
        return -EFAULT;
    }

    kbuf[len] = '\0';
    spin_lock_irqsave(&circ_buffer.lock, flags);
    
    // Si el buffer está lleno, liberamos la entrada más antigua
    if (circ_buffer.count == MAX_ENTRIES) {
        kfree(circ_buffer.entries[circ_buffer.tail]);
        circ_buffer.tail = (circ_buffer.tail + 1) % MAX_ENTRIES;
        circ_buffer.count--;
    }
    
    // Asignamos el nuevo mensaje
    if (circ_buffer.entries[circ_buffer.head]) {
        kfree(circ_buffer.entries[circ_buffer.head]);
    }
    
    circ_buffer.entries[circ_buffer.head] = kbuf;
    circ_buffer.head = (circ_buffer.head + 1) % MAX_ENTRIES;
    circ_buffer.count++;
    
    spin_unlock_irqrestore(&circ_buffer.lock, flags);  
    //increment_count();
    kfree(kbuf);

    return len;
}