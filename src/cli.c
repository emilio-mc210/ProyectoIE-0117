/*Headers:
*<unistd.h>: para open(), close(), read() y write()
*<unistd.h>: para flags
*VRGCLI: habilita funcionalidad CLI de vrg.h
*/
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h> 
#include<fcntl.h>
#define VRGCLI
#include "vrg.h"

/*Configuración del dispositivo:
*DEVICE_PATH: ruta del dispositivo creado por el driver
*ENTRY_SIZE: define el tamaño máximo en bytes para cada entrada del buffer 
*MAX_ENTRIES: define la capacidad máxima de mensjaes en el buffer circular
*/
#define DEVICE_PATH "/dev/chardev"
#define ENTRY_SIZE 128
#define MAX_ENTRIES 10

/*Función para leer el contenido del dispositivo:
*last_only: bandera para leer solo el último mensaje(1) o todos(0)
*Abre el dispositivo en modo lectura  o lectura/escritura
*EN vñia el comando "LAST" si last_only es verdaderp
*Lee el contenido del buffer
*Le da formato a la salida y la muestra. 
*/
void read_chardev(int last_only){
	/*Buffer sificiente para todas las entradas
	*fd: descriptor de archivo
	*bytes_read"bytes leídos 
	*flags: modo de apertura condicional 
	*/
	char buffer[ENTRY_SIZE*MAX_ENTRIES];
	int fd;
	ssize_t bytes_read;
	int flags = last_only ? O_RDWR : O_RDONLY;

	//Abrir char device solo para lectura
	fd = open(DEVICE_PATH, flags);
	if(fd == -1){
		fprintf(stderr, "Error: No se logro abrir el char device\n");
		return;
	}

	/*Manejo de comando LAST*/
	if (last_only){
		if (write(fd,"LAST", 4) == -1){
			fprintf(stderr, "Error al enviar comando\n");
			close(fd);
			return;
		}

	}

	/*Lee el contenido*/
	bytes_read = read(fd, buffer, sizeof(buffer)-1); //Dejar espacio para \0
	if(bytes_read == -1){
		fprintf(stderr, "Error: No se logro leer el char devicee\n");
		return;
	}

	/*Formato y visualización de la salida:
	*Agrega terminación nula 
	*Agrega salto de linea al final*/
	if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
        if (buffer[bytes_read-1] != '\n') {
            printf("\n");
        }
    }

	/*Cerrar el descriptor de archivo*/
	close(fd);
}

/*Función para escribir en el dispositivo: 
*Asigna memoria a cada entrada
*Usa snprintf para dar formato de forma segura
*/
void write_entry(const char *input){
	int fd;
	char *msg = NULL;
	size_t msg_len;

	//Abrir char device solo para escritura
	fd = open(DEVICE_PATH, O_WRONLY);
	if(fd == -1){
		fprintf(stderr, "Error: No se logro abrir el char device\n");
		return;
	}
	//Da formato al mensaje, se agrega +2 por +1 para /n y +1 para /0
	msg_len = strlen(input) + 2;
    msg = malloc(msg_len);

	//Verifica que se asigne la memoria correctamente 
    if (!msg) {
        fprintf(stderr, "Error: No se pudo asignar memoria\n");
        close(fd);
        return;
    }
    
    snprintf(msg, msg_len, "%s\n", input);

    if (write(fd, msg, strlen(msg)) == -1) {
        fprintf(stderr, "Error al escribir en el dispositivo\n");
    }
	free(msg);
	close(fd);
	}

/*FUnción para contar las entradas:
*Lee todo el contenido del buffer
*Divide cada entrada usando strok()
*Toma cada linea como una entrada*/
void count_entries() {

	/*buffer: buffer para almacenar las entradas leídas
	*count: contador para las entradas */
    char buffer[ENTRY_SIZE * MAX_ENTRIES];
    int fd; 
    ssize_t bytes_read;  
    int count = 0; 

	/*Abre el dispositivo en modo lectura y si falla imprime error*/
    fd = open(DEVICE_PATH, O_RDONLY); 
    if(fd == -1){ 
        fprintf(stderr, "Error: No se logró abrir el char device\n");
        return;
    }

	/*Lee los datos del dispositivo y los almacena en buffer. -1 para dejar espacio para el caracter nulo
	*Cierra el dispositivo despues de leerlo
	*Si no se leyó nada se muestra 0 entradas
	*/
    bytes_read = read(fd, buffer, sizeof(buffer)-1); 
    close(fd); 
    if(bytes_read <= 0) {
        printf("Número de entradas: 0\n");
        return;
    }

	//Asegura que los datos leídos formen una cadena válida en C  terminando con el caracter nulo(Se hace para prevenir que siga leyendo datos fuera del buffer o zonas de memoria invalida)
    buffer[bytes_read] = '\0'; 

    /*La funcion strtok() divide el string en "tokens" usando como delimitador \n, cada token es una entrada
	*EL bucle itera mientras strok() siga encontrando líneas e incrementa el contador */
    char *token = strtok(buffer, "\n");
    while(token != NULL) { / 
        count++; 

        /*Continúa buscando en el buffer desde donde quedó la última vez, devolviendo el siguiente token.
		*NULL indica que debe seguir procesando el mismo string.
    	*Cuando no hay más tokens, devuelve NULL y el bucle termina.
		*/ 
		token = strtok(NULL, "\n");
    }
    printf("Número de entradas: %d\n", count);
}

/*Función para limpiar todas entradas:
*Envía el comando especial "CLEAR" que el driver interpreta para liberar todas las entradas del buffer circular.
 */
void clean_device(void) {
    int fd = open(DEVICE_PATH, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: No se pudo abrir el char device\n");
        return;
    }
    if (write(fd, "CLEAR", 5) < 0) {
        fprintf(stderr, "Error al enviar comando de limpieza\n");
    }
    close(fd);
}

int main(int argc, char *argv[]){
	vrgcli("Prueba de CLI v0.1"){

		//Muestra ayuda
		vrgarg("-h\tDesplegar ayuda"){
			vrgusage();
		}

		//Mestra el último mensaje
		vrgarg("-l\tMostrar ultimo mensaje"){
			read_chardev(1);
		}

		//Limpiar buffer
		vrgarg("--clean\tLimpiar el buffer"){
			printf("Limpiando el char device)\n");
			clean_device(); 
		}

		//Leer el device
		vrgarg("-r\tLeer el char device"){
			printf("Leyendo dispositivo:\n");
			read_chardev(0);
		}

		//Contar las entradas del device
		vrgarg("--count\tContar las entradas del device"){
			count_entries(); 
		}
		
		//Escribir una entrada en el char device (Argumento opcional)
		vrgarg("[message]\tThe string to write on the char device"){
			printf("Escribiendo: %s\n", vrgarg);
			write_entry(vrgarg);
		}

		//Error: Argumento no esperado
		vrgarg(){
			vrgusage("Unexpected argument: %s\n", vrgarg);
		}
	}
	return 0;
}
