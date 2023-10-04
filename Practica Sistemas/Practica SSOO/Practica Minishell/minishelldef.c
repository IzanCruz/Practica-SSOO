#include <stdlib.h>
#include <stdio.h>
#include "parser.h"
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>

/*void ejecutar_comandos(tline *line){
	int i; 
	pid_t pid;
	 
	for (i=0;i<line->ncommands;i++){
		pid=fork();
		if (pid<0){
			fprintf(stderr,"Error Fork");
			exit(1);
		}
		
		if (pid == 0){
			//HIJO
			if (line->commands[i].filename == NULL){
				fprintf(stderr,"%s: no se encuentra el mandato\n", line->commands[i].argv[0]);
			}
			execv(line->commands[i].filename,line->commands[i].argv);
			exit(1);	
		}
	}
	for (i=0;i<line->ncommands;i++){
		wait(NULL);
		
	}
}*/

void mandatocd(tline *line){
	char *home;
	int e;
		if(line->commands[0].argv[1] != NULL){
			e = chdir(line->commands[0].argv[1]);
			if (e<0){
				fprintf(stderr,"%s: Directorio Incorrecto",line->commands[0].argv[1]);
			}
			else{
				printf("Cambiando a directorio: %s", line->commands[0].argv[1]);
			}
		}
		else{
			home = getenv("HOME");
			e = chdir(home);
			if (e<0){
				fprintf(stderr,"%s: Directorio Incorrecto",home);
			}
			else{
				printf("Cambiando a directorio: %s", home);
			}
		}
	
	
}

int main(int argc, char *argv[]){
	
	char buffer[1024];
	tline *line; 
	
	printf("$ ");
	while(fgets(buffer,1024,stdin) != NULL){
		
		printf("$ ");
		line=tokenize(buffer);
		if (line != NULL){
			if (line->ncommands == 1)){
				if (strcmp(line->commands[0].argv[0], "cd") == 0){
					mandatocd(line);
					}
				else if (strcmp(line->commands[0].argv[0], "exit") == 0){
					exit(0);
				}
				else{
					 ejeccomando(line);
				}
			}
					
			else{
			ejecutar_comandos(line); 	 
		}
	}
	}
	return 0;	

}


void ejeccomando(tline *line){
	pid_t pid;
	int status;
	
	pid = fork(); 
	if (pid < 0) { /* Error */
		fprintf(stderr, "Falló el fork()"); 
		exit(1); 
	}
	else if (pid == 0) { /* Proceso Hijo */
	 execvp(line->commands[0].filename, line->commands[0].argv ); 
	 //Si llega aquí es que se ha producido un error en el execvp 
	 printf("Error al ejecutar el comando: %s\n", line->commands[0].filename); 
	 exit(1); 
	 
	 } 
	else { /* Proceso Padre. 
	 - WIFEXITED(estadoHijo) es 0 si el hijo ha terminado de una 
	manera anormal (caida, matado con un kill, etc). 
	 Distinto de 0 si ha terminado porque ha hecho una llamada a la 
	función exit() 
	 - WEXITSTATUS(estadoHijo) devuelve el valor que ha pasado el hijo 
	a la función exit(), siempre y cuando la 
	 macro anterior indique que la salida ha sido por una llamada a 
	exit(). */ 
	 wait (&status); 
		if (WIFEXITED(status) != 0){
			if (WEXITSTATUS(status) != 0){ 
				printf("El comando no se ejecutó correctamente\n");
			}
		}
	 } 
}



