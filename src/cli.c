#include<stdio.h>
#include<stdlib.h>
#include<unistd.h> //Para open() y close()
#include<fcntl.h> //Para flags
#define VRGCLI
#include "vrg.h"

//Mismo nombre del char device que se crea
#define DEVICE_PATH "/dev/chardev"
#define ENTRY_SIZE 128
#define MAX_ENTRIES 10

void read_chardev(int last_only){
	char buffer[ENTRY_SIZE*MAX_ENTRIES];
	int fd; //File descriptor
	ssize_t bytes_read;
	int flags = last_only ? O_RDWR : O_RDONLY;

	//Abrir char device solo para lectura
	fd = open(DEVICE_PATH, flags);
	if(fd == -1){
		fprintf(stderr, "Error: No se logro abrir el char device\n");
		return;
	}
	if (last_only){
		if (write(fd,"LAST", 4) == -1){
			fprintf(stderr, "ERror al enviar comnado\n");
			close(fd);
			return;
		}

	}
	//Leer informacion del archivo al buffer
	bytes_read = read(fd, buffer, sizeof(buffer)-1); //Dejar espacio para \0
	if(bytes_read == -1){
		fprintf(stderr, "Error: No se logro leer el char devicee\n");
		return;
	}
	if (bytes_read > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
        if (buffer[bytes_read-1] != '\n') {
            printf("\n");
        }
    }

 


	//Terminar el buffer con \0 para tratarlo como string
	//buffer[bytes_read] = '\0';

	//Imprimir contenido 
	//printf("%s",buffer);
	//printf("\n");

	//Cerrar el descriptor de archivo
	close(fd);
}

void write_entry(const char *input){
	int fd; //File descriptor
	char *msg = NULL;
	size_t msg_len;

	//Abrir char device solo para escritura
	fd = open(DEVICE_PATH, O_WRONLY);
	if(fd == -1){
		fprintf(stderr, "Error: No se logro abrir el char device\n");
		return;
	}
	msg_len = strlen(input) + 2; // +1 para \n y +1 para \0
    msg = malloc(msg_len);
    if (!msg) {
        fprintf(stderr, "Error: No se pudo asignar memoria\n");
        close(fd);
        return;
    }
    
    snprintf(msg, msg_len, "%s\n", input);
    
    if (write(fd, msg, strlen(msg)) == -1) {
        fprintf(stderr, "Error al escribir en el dispositivo\n");
    }

	
	//Escribir en el buffer
	/*char *msg = (char *)malloc(sizeof(char *)*(strlen(input)+1)); //Para el \n
	
	strcat(msg, input);
	strcat(msg, "\n");
	write(fd, msg, strlen(msg));*/
	//Liberar memoria
	free(msg);

	//Cerrar el descriptor de archivo
	close(fd);
	}

void count_entries() {
    char buffer[ENTRY_SIZE * MAX_ENTRIES];
    int fd;
    ssize_t bytes_read;
    int count = 0;

    fd = open(DEVICE_PATH, O_RDONLY);
    if(fd == -1){
        fprintf(stderr, "Error: No se logró abrir el char device\n");
        return;
    }

    bytes_read = read(fd, buffer, sizeof(buffer)-1);
    close(fd);

    if(bytes_read <= 0) {
        printf("Número de entradas: 0\n");
        return;
    }

    buffer[bytes_read] = '\0';

    // cuenta las entradas del device, cada entrada termina con un salto de linea
    char *token = strtok(buffer, "\n");
    while(token != NULL) {
        count++;
        token = strtok(NULL, "\n");
    }

    printf("Número de entradas: %d\n", count);
}

//Para limpiar las entradas
void clean_device(void) {
    int fd = open(DEVICE_PATH, O_WRONLY);
    if (fd == -1) {
        fprintf(stderr, "Error: No se pudo abrir el char device\n");
        return;
    }

    // Envía el comando especial "CLEAR" al driver
    if (write(fd, "CLEAR", 5) < 0) {
        fprintf(stderr, "Error al enviar comando de limpieza\n");
    }

    close(fd);
}

int main(int argc, char *argv[]){
	vrgcli("Prueba de CLI v0.1"){
		//Imprimir ayuda
		vrgarg("-h\tDesplegar ayuda"){
			vrgusage();
		}

		//Ultimo mensaje
		vrgarg("-l\tMostrar ultimo mensaje"){
			printf("Ultimo mensaje (return o algo)\n");
			read_chardev(1);
		}

		//Limpiar buffer
		vrgarg("--clean\tLimpiar el buffer"){
			printf("Limpiar buffer (return o algo)\n");
			clean_device(); 
		}

		//Leer el device
		vrgarg("-r\tLeer el char device"){
			printf("Leyendo dispositivo:\n");
			read_chardev(0);
		}

		//Contar las entradas del device
		vrgarg("--count\tContar las entradas del device"){
			printf("Contar entradas\n");
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
