/*Headers:
*<linux/fs.h>: define estructuras y funciones para manejar archivos dentro del kernel 
*include<linux/uaccess.h>: funciones para copiar datos entre el espacio de usuario y el kernel 
*include<linux/cdev.h>: manjar char devices 
*include <linux/slab.h>: manejo de momoria dinámica en el kernel  
*include"last.h": gestiona el último mensaje ingresado en el buffer 
*include <linux/spinlock.h>: implementa spinlocks para protección contra accesos concurrentes. Se implementa para manejar el acceso al buffer circular 
*include"chardev.h": fnciones principales del driver del char device 
*/
#include<linux/fs.h> 
#include<linux/uaccess.h> 
#include<linux/cdev.h>
#include <linux/slab.h> 
#include"last.h"
#include <linux/spinlock.h> 
#include"chardev.h" 
 
/*Variables globales: 
*major: variable para almacenar el número asiganado por el kernel para identificar el char device
*char_class: puntero a una clase de dispositivo, permite que udev cree el nodo en /dev/chardev y crea un directorio en /sys/class/
*char_device: puntero al dispositivo dentro de la clase char_class para representar al char device, 
permite crear el archivo en /dev/chardev y vincula el major/minor number con las operaciones del driver. Se inicializa en NULL para evitar errores
*/
static char command_mode[16] = "";
static int major; //Número que el kernel asigna para identificar el chardevice 
static struct class *char_class = NULL;
static struct device *char_device = NULL; 

/*Estructura del buffer circular:
*entries: arreglo de punteros a strings(las entradas del buffer) de tamaño MAX_ENTRIES
*head: indice de escritura. Indica dónde se inserta la proxima entrada(primera entrada del buffer)
*count: contador de los mensajes actuales del buffer 
*lock: spinlock para prevenir el acceso simultáneo al buffer 
*/
static struct {

    char *entries[MAX_ENTRIES]; 
    int head; 
    int tail; 
    int count; 
    spinlock_t lock; 

} circ_buffer; 

/*Estructura de peraciones del device
*open: función llamada cuando se abre el dispositivo
*release: función llamada cuando se cierra el dispositivo 
*read: función llamda cuando se lee el dispositivo
*write: función llamada cuando se escribe al dispositivo 
*/
static struct file_operations fops = {
	.open = dev_open, 
	.release = dev_release, 
    .read = dev_read, 
    .write = dev_write
};

//Función para inicializar el char device
int init_chardev(void) {

    int i; //Variable para el loop de inicialización 

    /*Inicializa el spinlock para proteger el buffer 
    *Inicializa los punteros de escritura y lectura en 0
    *Inicial el contador de mensajes en 0
     */
    spin_lock_init(&circ_buffer.lock);
    circ_buffer.head = 0; 
    circ_buffer.tail = 0; 
    circ_buffer.count = 0; 

    /*Bucle para limpiar todas las entradas del buffer marcando cada posición como vacía con NULL
     */
    for (i = 0; i < MAX_ENTRIES; i++) { 
        circ_buffer.entries[i] = NULL; 
    }

    /*Registra el dispositivo de caracteres en el kernel:
    *0: solicita asignación dinámica del major number
    *DEVICE_NAME: asocia el nombre del dispositivo con sus operaciones
    *&fops: puntero a struct file_operations con las funciones del driver
    *
    *El condicional verifica si el registro tuvo exito:
    *En caso de error registra el mensaje de error con prioridad ALERT en logs del kernel
    *Retorna valores negativos en caso de error
    *
    *Si el registro fue exitoso: 
    *Genera la notificación en logs, utiliza KERN_INFO para prioridad de mensajes informativos
    *Registra el major number asignado para referencia futura
    */
    major = register_chrdev(0, DEVICE_NAME, &fops); 
    if (major < 0) { 
        printk(KERN_ALERT "Modulo: Registro del char device fallo con %i\n", major);
        return major;
    }
    printk(KERN_INFO "Modulo: Registro de char device exitoso con numero mayor %i\n", major);

    /*Crea una clase de dispositivo en sysfs
    *Permite que udev cree automáticamante el nodo en /dev
    *Crea un directorio en /sys/class/ con el nombre del dispositivo
    *Establece los atributos base para la administración de dispositivos
    */
	char_class = class_create(DEVICE_NAME);


        /*Verificación de errores en la creación de la clase: 
        *IS_ERR(): detecta punteros de error del kernel
        *Si hay un error desregistra el char device(desregistra el major number) 
        *Retorna el error de creación de clase. PTR_ERR lo convierte en un valor númerico  
        * */
        if (IS_ERR(char_class)) { 
        unregister_chrdev(major, DEVICE_NAME); 
        return PTR_ERR(char_class);
    }
 
    /*Crea el dispositivo dentro de la calse:
     *char_class: clase a la que pertenece el device 
     *MKDEV(major,0): crea el número de dispositivo
     *DEVICE_NAME: nombre del dispositivo en /dev
     *Se utiliza NULL para los espacios que no se necesitan 
     *
     *Crea la entrada en /sys/class
     *Genera evento uevent para que udev cree el nodo en /dev
     *Devuelve puntero al device creado
     */
    char_device = device_create(char_class, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

    /*Verificación de errores al crear el dispositivo:
    *IS_ERR: detecta punteros de error del kernel, aunque se haya creado la clase la creacion del dispositivo puede fallar
    *SI hay un error destruye la clase y luego desregistra el dispositivo
    *Convierte el codigo de error a código númerico con PTR_ERR()
    *Si no hubo ningun error se imprime un mensaje indicando que el device se creó 
    */
    if (IS_ERR(char_device)) {  
        class_destroy(char_class); 
        unregister_chrdev(major, DEVICE_NAME);
        return PTR_ERR(char_device); 
    }
    printk(KERN_INFO "Modulo: Char device creado en /dev/%s\n", DEVICE_NAME);
    return 0;
}

//Función para limpiar los recursos usados por el device
void cleanup_chardev(void) {

    /*Almacena el estado de las interrupciones (es necesario para spin_lock_irqsave) en flags
    *Desactiva las interrupciones locales en ese CPU y adquiere el spinlock para evitar que otros procesos accedan
    *Guarda el estado previo de interrupciones en "flags"
    **/
    unsigned long flags;
    spin_lock_irqsave(&circ_buffer.lock, flags);

    /*Bucle para recorrer todas las entrdas del buffer para que la liberación sea segura
    *Libera memoria del kernel con kfree
    *Por último indica que la entrada ya está vacía y se puede volver a utilizar 
    */
    for (int i = 0; i < MAX_ENTRIES; i++) {
        kfree(circ_buffer.entries[i]);
        circ_buffer.entries[i] = NULL;
    }

    /*Libera el spinlock y restaura el estado de interrupciones usando la varaible flags 
    */
    spin_unlock_irqrestore(&circ_buffer.lock, flags);

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

//Funcion de lectura del dispositivo 
ssize_t dev_read(struct file *filep, char *buffer, size_t len, loff_t *offset) {

    /*flags: variable para guardar el estado de las interrupciones  
    *output_buffer: variable para guardar el estado de las interrupciones, se inicializa vacío 
    *output_size: tamaño total necesario para todos los mensajes 
    *i, pos: variables de iteración  y posición
    *ret: variable de retorno para los bytes leídos o error 
    */
    unsigned long flags;
    char *output_buffer = NULL; 
    size_t output_size = 0;
    int i, pos;
    ssize_t ret = 0; 
    //size_t msg_len , to_copy;

    /*Protección de buffer circular: 
    *spin_lock_irqsave: bloquea el acceso al buffer y deshabilita interrupciones 
    *Se almacena el estados de interrupciones en flags para restaurarlo después
    */
    spin_lock_irqsave(&circ_buffer.lock, flags); //Bloquea el acceso al buffer para que no se use mientras se está leyendo del dispositivo y guarda el estado de las interrupciones en flags para restaurarlo despues
    
    /*Modo de operación "last": 
    *Se activa cuando command_mode contiene "last"
    *Solo devuelve el mensaje más reciente en el buffer
    *Requiere que haya al menos un mensaje (circ_buffer.count > 0)*/
    if (strcmp(command_mode, "last") == 0 && circ_buffer.count > 0){

         /*Cálculo de la posición del último mensaje:
         * circ_buffer.head apunta a la próxima posición disponible (head-1 es la última escrita)
         * Se suma MAX_ENTRIES antes del módulo para evitar valores negativos
         * % MAX_ENTRIES asegura el comportamiento circular del buffer
         */
        int last_position = (circ_buffer.head -1 + MAX_ENTRIES) % MAX_ENTRIES;

        /*Condicional para verificar que la posición contiene un mensaje válido:
        *Se cálcula el tamaño necesario con strlen, se le suma uno para el carácter nulo
        *Asigna memoria en el espacio kernel con kmalloc, GFP_KERNEL le da prioridad normal de asignación y se le suma 1 para el salto de línea
        */
        if (circ_buffer.entries[last_position]){
            output_size = strlen(circ_buffer.entries[last_position]) + 1;
            output_buffer = kmalloc(output_size + 1, GFP_KERNEL);

            /*Verificación de asignación de memoria:
            *Si kmalloc falla al asignar memoria se libera el buffer con spin_unlock_irqrestore
            *Imprime error, memoria no hay memoria disponible 
            **/
            if (!output_buffer){
                spin_unlock_irqrestore(&circ_buffer.lock, flags);
                return -ENOMEM;
            }

            /*Construcción del mensaje de salida:
             *snprintf es seguro contra desbordamientos de buffer
             *Añade salto de línea al final con "%s\n"
             *output_size+1 como límite máximo garantiza no sobrepasar el buffer
             */
            snprintf(output_buffer, output_size + 1, "%s\n", circ_buffer.entries[last_position]);
        }
    }
    /*Modo normal. Concatena todas las entradas del buffer para mostrarlas */
    else {
        /*Bucle para calcular el tamaño total requerido para almacenar las entradas: 
        *Itera sobre todas las entradas válidas(0 a circ_buffer.count-1)
        *Para cada entrada, se suma +1 para el salto de línea
        *"pos" cálcula la posición actual
        *Con el condicional si existe un mensaje en esa posición sumamos su longitud y +1 por el salto de línea 
        */
       for (i = 0; i < circ_buffer.count; i++) { 
            pos = (circ_buffer.tail + i) % MAX_ENTRIES; 
            if (circ_buffer.entries[pos]) {
                output_size += strlen(circ_buffer.entries[pos]) + 1; 
            }
        }

        /*Condicional para verificar si hay mensajes en el buffer:
        *Se asigna el buffer de salida con +1 para el cáracter nulo final y GFP_KERNEL para asignación normal de memoria
        *Si no hay entradas en el buffer se desbloquea y se restaura el estado de las interrupciones
        */
       if (output_size > 0) { 
            output_buffer = kmalloc(output_size + 1, GFP_KERNEL);//Si el buffer está vacío se desbloquea 
            if (!output_buffer) {
                spin_unlock_irqrestore(&circ_buffer.lock, flags);
                return -ENOMEM;
                }

                /*Se inicializa el buffer como string vacío*/
                output_buffer[0] = '\0'; 

                /*COnstrucción del buffer de salida concatenando todas las entradas:
                *El bucle itera sobre als entradas válidas
                *"pos" cálcula la posición con % MAX_ENTRIES para aegurar un recorrido circular. Empieza desde el indice más antiguo con circ_byffer.tail
                *Se comprueba que la posición tiene un mensaje válido con el condicional
                *"strcat" concatena el mensaje actual al buffer de salida:
                *output_buffer: buffer destino donde se acumula todo el contenido
                *circ_buffer.entries[pos]: Cadena actual a agregar al buffer
                *strcat automáticamente encuentra el final de output_buffer,buscando el '\0,
                *copia la nueva cadena a partir de esa posición y agrega un nuevo '\0' al final del resultado
                */
                for (i = 0; i < circ_buffer.count; i++) {
                    pos = (circ_buffer.tail + i) % MAX_ENTRIES;
                    if (circ_buffer.entries[pos]){
                        strcat(output_buffer, circ_buffer.entries[pos]);
                    }
                }
            }   
        }
        /*Libera el buffer y restaura el estado de las interrupciones con flags después de terminar la concatenación*/
        spin_unlock_irqrestore(&circ_buffer.lock, flags); 
        
        /*Caso en que no hayan entradas para devolver*/
        if (!output_buffer) {
            return 0; 
        }

        /*Cálculo de bytes a copiar:
        *min() selecciona el menor entre:
        *   len: Lo que el usuario solicitó
        *   output_size - *offset: Lo que queda disponible por leer
        *Esto maneja correctamente lecturas parciales
        *Condicional para verificar que hay datos para copiar
        *Si no hay nada más para copiar se libera la memoria y retorna 0
        */
        size_t to_copy = min(len, output_size - *offset); 
        if (to_copy <= 0) {
            kfree(output_buffer);
            return 0;
        }
        
        /*Copiar al espacio usuario: 
        *Copy_to_user verifica:
        * Que el buffer de usuario es válido y escribible
        * Que el rango de memoria es accesible
        *EFAULT indica error al copiar, dirección invalida
        */
        if (copy_to_user(buffer, output_buffer + *offset, to_copy) != 0) {
            kfree(output_buffer);
            return -EFAULT;
        }

        /*Actualización de estado:
        *Incrementa offset para la próxima lectura
        *ret almacena los bytes efectivamente copiados
        */
        *offset += to_copy;
        ret = to_copy;

        /*Libera la memoria del buffer temporal*/
        kfree(output_buffer);

        /*Reset del modo "last":
        *Solo se activa una vez por comando "LAST"
        *strcpy segura porque command_mode tiene tamaño fijo (16 bytes)
        */
        if (strcmp(command_mode, "last") == 0) {
            strcpy(command_mode, "");
        }

        /*Retorna el número de bytes copiados o error*/
        return ret;
    }
     


//Funcion de esccritura en el dispositivo 
ssize_t dev_write(struct file *filep, const char *buffer, size_t len, loff_t *offset) {
    
    /*flags: almacena el estado de las interrupciones
    *kbuf: buffer temporal en el espacio kernel dónde se copian los datos del usuario*/
    unsigned long flags;
    char *kbuf;
    

    /*Condicional para validar la longitud de los datos:
    *Rechaza entradas vacías
    *REchaza datos mayores a ENTRY_SIZE para que los mensajes no excedan el tamaño máximo
    *Retorna error con EINVAL */
    if (len <= 0 || len > ENTRY_SIZE) {
        return -EINVAL;
    }

    /*Manejo de comando CLEAR:
    *Verifica que el mensaje es "CLEAR" 
    *Llama a la función para limpiar el buffer
    *Retrona exito */
    if (len == 5 && strncmp(buffer, "CLEAR", 5) == 0) {
        clear_chardev();
        return len;
    }

    /*Manejo del comando LAST:
    *Verifica que el mensaje es "LAST"
    *copia la cadena "last" en command_mode para activar el modo(se usa strcpy porque el origen es mas corto que el destino)
    *Garantiza la terminación nula de "last" para evitar errores si strcpy falla
    */
    else if ( len == 4 && strncmp(buffer, "LAST", 4) == 0){
        strcpy(command_mode, "last");
        command_mode[sizeof(command_mode)-1] = '\0';
        return len;
    }
    
    /*Asignación de memoria:
    *Se utiliza GFP_KERNEL para dar prioridad e asignación
    *Se reservan len+1 para incluir el terminador nulo
    *El condicional verifica que se haya asignado la memoria
    *En caso de error retorna error con ENOMEM
    */
    kbuf = kmalloc(len + 1, GFP_KERNEL);
    if (!kbuf) {
        return -ENOMEM;
    }
    
    /*Copia los datos desde el espacio usuario:
    *Copy_from_user verifica punteros y permisos
    *Libera la memoria reservada en caso de error
    *Retorna 0 en exito y número de bytes no copiados en error*/
    if (copy_from_user(kbuf, buffer, len) != 0) {
        kfree(kbuf); 
        return -EFAULT;
    }
    
    /*Añade la terminación nula
    */
    kbuf[len] = '\0';
    
    /*Protege el buffer de interrupciones y almacena el estado de las interrupciones en flags para restaurarlas
    */
    spin_lock_irqsave(&circ_buffer.lock, flags);
    
    /*Manejo del buffer lleno (política FIFO):
    *Si count == MAX_ENTRIES se elimina el mensaje más antiguo
    *Libera la memoria de la entrada tail(última entrada) y la marca como vacía con NULL
    *Actualiza tail circularmente, moviendola un espacio
    *Decrementa el contador count
    */
    if (circ_buffer.count == MAX_ENTRIES) { 
        kfree(circ_buffer.entries[circ_buffer.tail]); 
        circ_buffer.entries[circ_buffer.tail] = NULL; 
        circ_buffer.tail = (circ_buffer.tail + 1) % MAX_ENTRIES; 
        circ_buffer.count--;
    }
    
    /*Guardar un nuevo mensaje en el buffer:
    *El condicional libera la primera entrada(head) si ya contenía un mensaje
    *Se almacena el nuevo mensaje en head
    *Actualza head circularmente
    *Incrementa el contador count
    *Actualzia el último mensaje con set_last_message
    */
    if (circ_buffer.entries[circ_buffer.head]) { 
        kfree(circ_buffer.entries[circ_buffer.head]);
    }
    circ_buffer.entries[circ_buffer.head] = kbuf;
    set_last_message(kbuf);
    circ_buffer.entries[circ_buffer.head] = kbuf; 
    circ_buffer.head = (circ_buffer.head + 1) % MAX_ENTRIES; 
    circ_buffer.count++;
    
    /*Libera el buffer y restaura el estado de las interrupciones 
    *Si se tuvo exito retorna el número de bytes escritos 
    */
    spin_unlock_irqrestore(&circ_buffer.lock, flags);
    return len; 
}


/*Función para abrir el dispositivo:
*inode: puntero a la estructura inode del archivo del dispositivo(struct cdev)
permite obtener el número mayor y menos del dispositivo
*
*filep: puntero a la estructura file que representa el archivo abierto
mantiene el estado del archivo mientras el dispositivo esté abierto
*
*Registra en en logs del kernel que se abrió el dispositivo
*/
int dev_open(struct inode *inode, struct file *filep){
	printk(KERN_INFO "Modulo: Mayor: %i, Menor: %i\n", imajor(inode), iminor(inode));
	return 0;
}

/*Función para cerrar el dispositivo:
*filep: puntero a la estructura file que se está cerrando
*Libera recursos asignados en dev_open() y limpia cualquier estado especifico del archivo
*Registra en logs del kernel que el archivo se cerró
*/
int dev_release(struct inode *inode, struct file *filep){
	printk(KERN_INFO "Modulo: Archivo cerrado");
	return 0;
}

/*Función para limpiar el buffer*/
void clear_chardev(void) {
    /*Protección de buffer circular: 
    *flags: variable para almacenar el estado de las interrupciones
    *spin_lock_irqsave: bloquea el acceso al buffer y deshabilita interrupciones 
    *Almacena el estado de las interrupciones en flags para restaurarlo después
    */
    unsigned long flags;
    spin_lock_irqsave(&circ_buffer.lock, flags);
    
    /*Bucle para liberar las entradas del buffer:
    *Recorre cada entrada del buffer circular, libera la memoria y reemplaza cada entrada con NULL
    *Despues s reinicia los índices y el contador a 0
    */
    for (int i = 0; i < MAX_ENTRIES; i++) {
        kfree(circ_buffer.entries[i]);
        circ_buffer.entries[i] = NULL;
    }
    circ_buffer.head = 0;
    circ_buffer.tail = 0;
    circ_buffer.count = 0;
    
    /*Libera el buffer y restaura el estado de las interrupciones 
    *Registra un mensaje unformativo en logs del kernel de que se limpió el buffer correctamente  
    */
    spin_unlock_irqrestore(&circ_buffer.lock, flags);
    printk(KERN_INFO "Modulo: Buffer limpiado completamente\n");
}

