//Modulo que inicializa el char device y lo puede limpiar
#include<linux/module.h>
#include<linux/init.h>
#include"chardev.h"

static int __init mymodule_init(void) {
    return init_chardev();
}

static void __exit mymodule_exit(void) {
    cleanup_device();
}

module_init(mymodule_init);
module_exit(mymodule_exit);

MODULE_LICENSE("GPL");