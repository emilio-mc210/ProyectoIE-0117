//Archivo para gestionar el almacenamiento y recuperación del último mensaje en el char device

#include <linux/slab.h> //Para kstrdup() y kfree()
#include <linux/string.h> //Para funciones de manejo de strings
#include "last.h"

//Variable para este archivo que guarda el ultimo mensaje
static char *last_msg = NULL;

//Almacena una copia del mensjae como el último recibido
void set_last_message(const char *msg) {
    kfree(last_msg);

    //Duplica un string usando memoria dinamica en kernel 
    last_msg = kstrdup(msg, GFP_KERNEL);
}

//Recupera el último mensaje almacenado
const char* get_last_message(void) {
    return last_msg;
}