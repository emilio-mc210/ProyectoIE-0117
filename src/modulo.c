/*Modulo del kernel para dispositivos de careacteres: 
*<linux/module.h>: para macros MODULE_*, module_init/exit
*<linux/init.h>: para las macros __init y __exit
*"chardev.h": para las funciones del driver
*/
#include<linux/module.h>
#include<linux/init.h>
#include"chardev.h"

/*Función de inicialización del módulo
 *Es llamada por el kernel al cargar el módulo (insmod)
 *Autoriza la inicialización a init_chardev()
 *Propaga cualquier error de inicialización
 *La macro __init le indica al kernel que esta función puede ser descartada después de la inicialización para ahorrar memoria.
 */
static int __init mymodule_init(void) {
    return init_chardev();
}

/*Funcion para limpiar el modulo: 
 *Es llamada por el kernel al descargar el módulo (rmmod)
 *Delega la limpieza a cleanup_chardev()
 *No puede fallar (tipo void)
 *La macro __exit le indica al kernel que esta función solo se usa al descargar el módulo y puede ser optimizada en módulos embebidos.
 */
static void __exit mymodule_exit(void) {
    cleanup_chardev(); //el nombre está incorrecto
}

/*Registro del modulo:
 *module_init/moule_exit especifican las funciones de entrada/salida:
 * mymodule_init se ejecutará al cargar (insmod)
 * mymodule_exit se ejecutará al descargar (rmmod)
 */
module_init(mymodule_init);
module_exit(mymodule_exit);

/*MODULE_LICENSE especifica la licencia GPL para permitir enlazado con el kernel*/
MODULE_LICENSE("GPL");