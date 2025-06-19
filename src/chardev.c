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

//Función para inicializar el char device
int init_chardev(void) {
    int i; //Variable para el loop de inicialización 

    spin_lock_init(&circ_buffer.lock); //Inicializa el spinlock para proteger el buffer
    circ_buffer.head = 0; //Inicia el puntero de escritura en la posición 0
    circ_buffer.tail = 0; //Inicia el puntero de lectura en la posición 0
    circ_buffer.count = 0; //Inicia el contador de mensajes en 0 

    for (i = 0; i < MAX_ENTRIES; i++) { //Limpia todas las entradas del buffer 
        circ_buffer.entries[i] = NULL; //MArca cada posición como vacía 
    }

    major = register_chrdev(0, DEVICE_NAME, &fops); //Registra el chardev, se usa 0 como párametro para que el kernel asigne dinámicamente el numero mayor
    if (major < 0) { //Condicional para verificar si el registro tuvo exito. 
        printk(KERN_ALERT "Modulo: Registro del char device fallo con %i\n", major);
        return major;  //No encontro un numero valido
    }
    //Si se completó el registro se imprime un mensaje informativo en el log del kernel con prioridad INFO
    printk(KERN_INFO "Modulo: Registro de char device exitoso con numero mayor %i\n", major);

    char_class = class_create(DEVICE_NAME);

	if (IS_ERR(char_class)) { //Verifica si hubo un error al crear la clase, con IS_ERROR se detectan errores de punteros en el kernel 
        unregister_chrdev(major, DEVICE_NAME); //Si hubo un error, primero se desregistra el char device
        return PTR_ERR(char_class); //Retorna el código de error convertido en un valor numérico con PTR_ERROR
    }

    //Crea el dispositivo dentro de la calse que se creo anteriormente 
    /**Se utilizan los siguientes parametros:
     *char_class, la clase a la que pertenece el device 
     *MKDEV(major,0), crea el número de dispositivo
     *DEVICE_NAME, el nombre del dispositivo
     *Se utiliza NULL para los espacios que no se necesitan 
     */
    char_device = device_create(char_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

        //Si hubo un error se destruye la clase creada y desregistra el char device 
        class_destroy(char_class); 
        unregister_chrdev(major, DEVICE_NAME);

        return PTR_ERR(char_device); //Retorna el código de error
    }

    //Si no hubo ningun error se imprime un mensaje indicando que el device se creó 
    printk(KERN_INFO "Modulo: Char device creado en /dev/%s\n", DEVICE_NAME);

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

    printk(KERN_INFO "Modulo: Modulo desmontado correctamente.\n");
    printk(KERN_INFO "Modulo: Chardev con numero mayor %i eliminado correctamente", major);
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


    if (len <= 0 || len > ENTRY_SIZE) { //Valida el tamaño de los datos. Rechaza el dato si el tamaño es invalido (vacío o mayor al limite )
        return -EINVAL; //Indica que el tamaño es invalido 
    }
    
    //Reservar memoria en el kernel 
    kbuf = kmalloc(len + 1, GFP_KERNEL); //Se pide memoria para guardar el mensaje
    /**Se usa kmalloc (equivalente de malloc en el kernel)
     * Se utiliza el flag GFP_KERNEL, el cual da prioridad para la asignación de memoria 
     * Se reserva len + 1 bytes para incluir el terminador nulo
      */
     
    if (!kbuf) { //Verifica que se haya asignado la memoria correctamente 
        return -ENOMEM;  //En caso de que no se haya asignado correctamente se utiliza ENOMEM(error de no memoria )
    }
    
    if (copy_from_user(kbuf, buffer, len) != 0) { //Copia datos desde el espacio usuario 
        kfree(kbuf); //Libera memoria reservada
        return -EFAULT; //Si fallo la copia retorna error
    }
    
    kbuf[len] = '\0'; //Añade el caracter nulo al final 
    
    spin_lock_irqsave(&circ_buffer.lock, flags); //Protege el buffer circular con spinlock 
    
    //Si el buffer está lleno se usa el uso de memoria FIFO(first in-first out)
    if (circ_buffer.count == MAX_ENTRIES) { //Si el buffer ya esá lleno(es igual a MAx_ENTRIES) se borra el ultimo mensaje (tail)
        kfree(circ_buffer.entries[circ_buffer.tail]); //Libera memoria 
        circ_buffer.entries[circ_buffer.tail] = NULL; //Marca la entrada como vacía 
        circ_buffer.tail = (circ_buffer.tail + 1) % MAX_ENTRIES; //Mueve tail un espacio 
        circ_buffer.count--; //Reduce el contador 
    }
    
    //Guarda el mensaje nuevo 
    if (circ_buffer.entries[circ_buffer.head]) { //Sí ya había un mensaje en head se libera
        kfree(circ_buffer.entries[circ_buffer.head]);
    }
    //Asigna el nuevo mensaje y actualiza los índices 
    circ_buffer.entries[circ_buffer.head] = kbuf; //Guarda el mensaje 
    circ_buffer.head = (circ_buffer.head + 1) % MAX_ENTRIES; //Mueve head un espacio 
    circ_buffer.count++; //Incrementa el contador 
    
    spin_unlock_irqrestore(&circ_buffer.lock, flags); //Libera el spinlock 
    
    return len; //Si se tuvo exito retornamos el número de bytes escritos 
}
