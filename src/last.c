//Para encontrar el ultimo mensaje
#include <linux/slab.h> //Para kstrdup()
#include <linux/string.h>
#include "last.h"

//Variable para este archivo que guarda el ultimo mensaje
static char *last_msg = NULL;

void set_last_message(const char *msg) {
    kfree(last_msg);

    last_msg = kstrdup(msg, GFP_KERNEL);//Duplica un string usando memoria dinamica en kernel 
}

const char* get_last_message(void) {
    return last_msg;
}