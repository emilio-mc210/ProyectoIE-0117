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

void read_chardev(){
	char buffer[ENTRY_SIZE*MAX_ENTRIES];
	int fd; //File descriptor
	ssize_t bytes_read;

	//Abrir char device solo para lectura
	fd = open(DEVICE_PATH, O_RDONLY);
	if(fd == -1){
		fprintf(stderr, "Error abriendo el char device");
		return;
	}
	
	//Leer informacion del archivo al buffer
	bytes_read = read(fd, buffer, sizeof(buffer)-1); //Dejar espacio para \0
	if(bytes_read == -1){
		fprintf(stderr, "Error leyendo el char device");
		return;
	}

	//Terminar el buffer con \0 para tratarlo como string
	buffer[bytes_read] = '\0';

	//Imprimir contenido 
	printf(" %s ",buffer);

	//Cerrar el descriptor de archivo
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
		}
		//Limpiar buffer
		vrgarg("--clean\tLimpiar el buffer"){
			printf("Limpiar buffer (return o algo)\n");
		}
		//Escribir en el device
		vrgarg("-w\tEscribir en el char device"){
			printf("Escribir en el dispositivo\n");
		}
		//Leer el device
		vrgarg("-r\tLeer el char device"){
			printf("Leer el dispositivo\n");
			read_chardev();
		}
		//Contar las entradas del device
		vrgarg("--count\tContar las entradas del device"){
			printf("Contar entradas\n");
		}
		//Error
		vrgarg(){
			vrgusage("Unexpected argument: %s\n", vrgarg);
		}
	}
	return 0;
}
