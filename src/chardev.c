#include<linux/fs.h> //Define estructuras y funciones para manejar archivos dentro del kernel 
#include<linux/uaccess.h> //Funciones para copiar datos entre el espacio de usuario y el kernel 
#include<linux/cdev.h> //Manjar char devices 
#include <linux/slab.h> //Manejo de momoria dinámica en el kernel  
#include"last.h" //Gestiona el último mensaje ingresado en el buffer 
#include <linux/spinlock.h> //Implementa spinlocks para protección contra accesos concurrentes. Se implementa para manejar el acceso al buffer circular 
#include"chardev.h" //Funciones principales del driver del char device 

#define DEVICE_NAME "chardev" //Nombre del char device como aparece en /proc/devices
#define ENTRY_SIZE 128 //Tamaño máximo en bytes que puede tener cada entrada en el buffer 
#define MAX_ENTRIES 10 //Capacidad máxima de entradas que puede almacenar el buffer 

//Variables globales para este archivo
static int major; //Número que el kernel asigna para identificar el chardevice 

static struct class *char_class = NULL; //Puntero a una clase de dispositivo, permite que udev cree el nodo en /dev/chardev y crea un directorio en /sys/class/

static struct device *char_device = NULL; /*Puntero al dispositivo dentro de la clase char_class para representar al char device, 
permite crear el archivo en /dev/chardev y vincula el major/minor number con las operaciones del driver. Se inicializa en NULL para evitar errores*/


//Arreglo de punteros, donde cada uno es un bloque de tamano ENTRY_SIZE
//static char *buffer[MAX_ENTRIES];

//EStructura del buffer circular 
static struct {

    char *entries[MAX_ENTRIES]; //Array de punteros para almacenar los mensajes(strings), cada elemento del array apunta a un mensaje

    int head; //Indice de escritura, indica donde se insertará el proximo mensaje. Se inserta en la primera entrada del buffer (head)
    int tail; //Indice de lectura, indica de donde se leera el mensaje más antiguo. Se lee la última entrada del buffer (tail)
    int count; //Contador de los mensajes actuales en el buffer 
    spinlock_t lock; //Previene el acceso simultáneo al buffer

} circ_buffer; 

//Operaciones del device
static struct file_operations fops = {
	.open = dev_open, //Funcion llamada cuando se abre el dispositivo
	.release = dev_release, //Funcion llamada cuando se cierra el char dev
    .read = dev_read, //Función llamada cuando se lee del dispositivo
    .write = dev_write //Función llamada cuando se escribe al dispositivo 
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

    
	char_class = class_create(DEVICE_NAME); //Crea una clse de dispositivo en sysfs para que se cree automáticamente el nodo en /dev (crea el dispositivo en /dev/chardev)

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
    if (IS_ERR(char_device)) { //Verificar si hubo un error al crear el dispositivo 

        //Si hubo un error se destruye la clase creada y desregistra el char device 
        class_destroy(char_class); 
        unregister_chrdev(major, DEVICE_NAME);

        return PTR_ERR(char_device); //Retorna el código de error
    }

    //Si no hubo ningun error se imprime un mensaje indicando que el device se creó 
    printk(KERN_INFO "Modulo: Char device creado en /dev/%s\n", DEVICE_NAME);

    return 0;
}

//Función para limpiar los recursos usados por el device
void cleanup_chardev(void) {
    unsigned long flags; // almacena el estado de las interrupciones (es necesario para spin_lock_irqsave)
    
    spin_lock_irqsave(&circ_buffer.lock, flags); //Desactiva las interrupciones locales en ese CPU 
    //y adquiere el spinlock para evitar que otros procesos accedan y por ultimo guarda el estado previo de interrupciones en "flags"

    for (int i = 0; i < MAX_ENTRIES; i++) { //Recorre todas las entrdas del buffer para que la liberación sea segura 
        kfree(circ_buffer.entries[i]); //Libera memoria del kernel asignada previamente con kmalloc
        circ_buffer.entries[i] = NULL; //Indica que la entrada ya está vacía y se puede volver a utilizar
    }

    spin_unlock_irqrestore(&circ_buffer.lock, flags); //Libera el spinlock y restaura el estado de interrupciones usando la varaible flags 

    /*Elimina el dispositivo de la clase
     *Borra el nodo en /dev/DEVICE_NAME
     *Dispara eventos udev para notificar a user-space que se bborró el chardevice*/
    device_destroy(char_class, MKDEV(major, 0));

     /*Elimina la clase del sistema:
     * Remueve el directorio en /sys/class/DEVICE_NAME
     * Libera recursos de la estructura class */
    class_destroy(char_class);

    /* Desregistra el char device:
     * Libera el número 'major' asignado
     * Elimina la asociación nombre -> operaciones (fops)
     * Se debe de hacer despues de device_destroy*/
    unregister_chrdev(major, DEVICE_NAME);

    printk(KERN_INFO "Modulo: Modulo desmontado correctamente.\n");
    printk(KERN_INFO "Modulo: Chardev con numero mayor %i eliminado correctamente", major);
}

//dev_read muestra la entrada mas antigua

//Funcion de lectura del dispositivo 
ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {
    unsigned long flags; //Variable para guardar el estado de las interrupciones 
    char *output_buffer = NULL; // BUffer temporal para guardar las entradas y concatenarlas 
    size_t output_size = 0; //Tamaño total necesario para todos los mensajes 
    int i, pos; //Variables de iteración  y posición 
    ssize_t ret = 0; //Varaible de retorno par los bytes leídos o error 


    spin_lock_irqsave(&circ_buffer.lock, flags); //Bloquea el acceso al buffer para que no se use mientras se está leyendo del dispositivo y guarda el estado de las interrupciones en flags para restaurarlo despues
    
     //Calcula el tamaño total necesario para todas las entradas
    for (i = 0; i < circ_buffer.count; i++) { //Calcula cuanto espacio se necesita para almacenar todos los mensjaes del buffer 
        pos = (circ_buffer.tail + i) % MAX_ENTRIES;  //Calcula la posición circular actual 
        if (circ_buffer.entries[pos]) {  //Sí existe un mensaje en esa posición sumanmos su longitud y +1 por el salto de línea
            output_size += strlen(circ_buffer.entries[pos]) + 1; // +1 para el '\n'
        }
    }

    if (output_size == 0) { //Si el buffer está vacío se desbloquea 
        spin_unlock_irqrestore(&circ_buffer.lock, flags);
        return 0;
    }

    output_buffer = kmalloc(output_size + 1, GFP_KERNEL); // Reserva memoria para almacenar todos los mensajes concatenados y +1 para el carcater nulo final ('\0')
                                                        //GFP_KERNEL se usa para prioridad de asignación estándar 
    if (!output_buffer) { //Si falla la asignación de memoria se desbloquea el buffer y retorna error 
        spin_unlock_irqrestore(&circ_buffer.lock, flags);
        return -ENOMEM; //Retorna error con error de no memoria (memoria insuficiente )
    }

    output_buffer[0] = '\0'; //Concatenación de las entradas. Se inicializa como string vacío 

    for (i = 0; i < circ_buffer.count; i++) { //Recorre el buffer para concatenar los mensajes 

        /**Calcula la posición real en el buffer circular usando aritmética modular:
         *circ_buffer.tail: Índice de la entrada más antigua (punto de inicio)
         *i: Desplazamiento desde el tail (0 a count-1)
         *MAX_ENTRIES: Tamaño total del buffer circular
         * 
         * La operación módulo (%) asegura que el índice "dé la vuelta" cuando alcanza
         * el final del buffer
         */
        pos = (circ_buffer.tail + i) % MAX_ENTRIES; 

        /**
             * Concatena la entrada actual al buffer de salida:
             * output_buffer: Buffer destino donde se acumula todo el contenido
             * circ_buffer.entries[pos]: Cadena actual a agregar
             * 
             * strcat automáticamente encuentra el final de output_buffer (busca el '\0'), copia la nueva cadena a partir de esa posición y agrega un nuevo '\0' al final del resultado
             */
        if (circ_buffer.entries[pos]) {
            strcat(output_buffer, circ_buffer.entries[pos]);
        }
    }
    spin_unlock_irqrestore(&circ_buffer.lock, flags); //Desbloquea el buffer luego de terminar la concatenación 

    size_t to_copy = min(len, output_size - *offset);   //Determina cuantos bytes se puede copiar al usuario. Lo minimo entre lo solicitado y lo disponible 
    if (to_copy <= 0) { //SI no hay nada más que copiar si libera la memoria y retorna 0 
        kfree(output_buffer);
        return 0;
    }
    
    if (copy_to_user(buffer, output_buffer + *offset, to_copy) != 0) {
        /** Copiar al espacio usuario
         * buffer, el destino (espacio usuario)
         * output_buffer + *offset, el origen (mensaje desde posición actual)
         * to_copy, la cantidad de bytes a copiar
         * retorna 0 si hubo éxito o diferente de 0 si hay error
         */
        kfree(output_buffer); //Libera la memoria del buffer temporal 
        return -EFAULT;
    }

    *offset += to_copy; //Actualiza el offset para la próxima lectura 
    ret = to_copy; //Guarda el número de bytes copiados 

    kfree(output_buffer); //Libera la memoria del buffer temporal 

    return ret; //Retorna el número de bytes copiados o error 
}

//Funcion de esccritura en el dispositivo 
ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    unsigned long flags; //Guarada el estado de las interrupciones 
    char *kbuf; //Buffer en el kernel donde se copian los datos del usuario 
    


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

/**
* Funcion para abrir el dispositivo
*
* @param inode permite obtener numero mayor y menor del dispositivo abierto
* @param filep solo se mantiene mientras el dispositivo este abierto 
**/
int dev_open(struct inode *inode, struct file *filep){
	printk(KERN_INFO "Modulo: Mayor: %i, Menor: %i\n", imajor(inode), iminor(inode));
	return 0;
}

/**
* Funcion para cerrar el char device
**/
int dev_release(struct inode *inode, struct file *filep){
	printk(KERN_INFO "Modulo: Archivo cerrado");
	return 0;
}
