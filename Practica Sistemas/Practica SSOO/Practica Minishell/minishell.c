/*
 * myshell.c
 *
 *  Created on: 6 abr. 2020
 *      Author: Yago Navarrete Martinez
 *      Author: Santiago Ramos Gomez
 */

//gcc myshell.c -static libparser.a -o myshell

#include "parser.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <signal.h>
#include <fcntl.h>
#include <wait.h>
#include <sys/stat.h>

//Variable global line en que se almacenara la linea de comandos
tline * line;

char WhereAmI[512];

void kill_child(int sig){
    printf("\n");
}

//Cabecera de un metodo desarrollado tras el main
void redirect_cd(int num, char* direccion);

int main(void){
	
	char buf[1024];
	int i;
	
	//Variables para ejecutar mandatos
	pid_t pid, wpid;
	int status;
	int ** pipes;
	
	//Entradas y salidas estandar para invertir la redireccion
	int Entrada = dup(fileno(stdin));
	int Salida = dup(fileno(stdout));
	int Error = dup(fileno(stderr));
	
	signal(SIGINT,SIG_IGN); //No sabemos como hacer como en la shell de linux, donde obtiene CTRL+C y te pone un salto de linea
	signal(SIGQUIT, SIG_IGN); //Tan solo ignoramos las señales
	
	getcwd(WhereAmI, sizeof(WhereAmI)); //Sirve para obtener la ubicación actual (por defecto sera la carpeta donde se ubique el ejecutable)
	printf("\033[1;31mmsh\033[0m>\033[1;34m~%s\033[0m# ",WhereAmI);
	
	while(fgets(buf, 1024, stdin)){

		//Actualizamos "line" con la informacion analizada de la linea que introduzca el usuario
		line = tokenize(buf);

		if(line==NULL){
			continue;
		}

		//Redirecciones pertinentes
		if(line->redirect_input != NULL){
			//Sin O_CREATE no funiona cuando rediriges la entrada a un archivo a crear de 0
			if(open(line->redirect_input , O_CREAT | O_RDONLY) == -1){
				fprintf( stderr , "%s : Fallo en redirect_input: %s\n" , line->redirect_input , strerror(errno));
				
			//Error redireccion de fichero
			} else {
				dup2(open(line->redirect_input , O_CREAT | O_RDONLY),fileno(stdin)); //Redireccion de entrada
			}
		}
		if(line->redirect_output != NULL){
			
		//Para las de escritura nos encontramos gran cantidad de FLAGS, muy resumidamente permisos, tanto de lectura como de escritura, para el USER, GROUP y OTHERS
		/*A continuacion, la informacion obtenida del manual de linux:
		S_IRWXU
			00700 user (file owner) has read, write and execute permission
		S_IRUSR
			00400 user has read permission
		S_IWUSR
			00200 user has write permission
		S_IXUSR
			00100 user has execute permission
		S_IRWXG
			00070 group has read, write and execute permission
		S_IRGRP
			00040 group has read permission
		S_IWGRP
			00020 group has write permission
		S_IXGRP
			00010 group has execute permission
		S_IRWXO
			00007 others have read, write and execute permission
		S_IROTH
			00004 others have read permission
		S_IWOTH
			00002 others have write permission
		S_IXOTH
			00001 others have execute permission
		*/
		
			if(creat (line->redirect_output ,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ) == -1){
				fprintf( stderr , "%s : Fallo en redirect_output:  %s\n" , line->redirect_output , strerror(errno)); //Error redireccion de fichero
			} else {
			dup2(creat (line->redirect_output ,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH ),fileno(stdout)); //Redireccion de salida
			}
		}
		
		if(line->redirect_error != NULL){
			if(creat (line->redirect_error  ,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH) == -1){
				fprintf( stderr , "%s : Fallo en redirect_error: %s\n" , line->redirect_error , strerror(errno)); //Error redireccion de fichero
			} else {
				dup2(creat (line->redirect_error  ,  S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH),fileno(stderr)); //Redireccion de salida de error
			}
		}

		if(line->background){
			printf("Comando a ejecutarse en background (Funcionalidad no implementada)\n"); //No se ha implementado poder realizar comandos en background (ni "jobs" ni "fg")
			exit(1);
		}

		//Para solo un comando se contempla "cd" y "exit"
		if(line->ncommands==1){
			
			if(strcmp(line->commands[0].argv[0], "cd") == 0){ //Comprueba si es cd
				redirect_cd(line->commands[0].argc, line->commands[0].argv[1]);
				
			} else if(strcmp(line->commands[0].argv[0], "exit") == 0){ //Comprueba si es exit
				exit(0);

			} else { //Ejecuta el comando similar al del ejercicio "ejecuta" del tema 4
			
				//Dejamos de ignorar las señales
				signal(SIGINT,(void (*)(int))kill_child);
				signal(SIGQUIT, (void (*)(int))kill_child);
				
				pid = fork();
				
				if(pid < 0){
					fprintf(stderr, "Falló el fork().\n%s\n", strerror(errno)); //Error en el fork
					exit(1);

				} else if(pid == 0){

					if(line->commands[0].filename!=NULL){ //Validamos el comando
						execvp(line->commands[0].filename, line->commands[0].argv );
						// Si llega al fprintf es porque el execvp no se ha ejecutado correctamente
						fprintf(stderr, "Error al ejecutar el comando: %s\n", strerror(errno));
					} else {
						fprintf(stderr, "%s : No se encuentra el mandato\n" , line->commands[0].argv[0]); //Error el mandato no se encuentra
						return 0;
					}
				} else {
					wait(&status);
					if(WIFEXITED(status) != 0){
						if(WEXITSTATUS(status) != 0){
							printf("El comando no se ejecutó correctamente\n");
						}
					}
				}
			}
			
		//Para dos o mas comandos
		} else {
			
			//Dejamos de ignorar las señales
			signal(SIGINT,(void (*)(int))kill_child);
			signal(SIGQUIT, (void (*)(int))kill_child);

			//Memoria dinamica para los pipes entre comandos
			pipes = (int**) malloc((line->ncommands-1)*sizeof(int*));
			for(i = 0; i < line->ncommands - 1; i++){
				pipes[i] = (int*) malloc(2*sizeof(int));
				pipe(pipes[i]);
			}
			
			for(i = 0; i < line-> ncommands; i++){
				
				if(line->commands[i].filename != NULL){ //Se comprueba que todos sean validos
				
					//Se hace un fork para cada proceso hijo y se actualiza el pid
					pid=fork();
					
					if(pid<0){ //Error en el fork
						fprintf(stderr, "Falló el fork().\n%s\n", strerror(errno));
						exit(1);
						
					} else if(pid==0){
						
						//Primer hjijo
						if(i == 0){
							close(pipes[i][0]);
							dup2(pipes[i][1],fileno(stdout)); //Solo salida
							execvp(line->commands[i].argv[0], line->commands[i].argv);
							
							fprintf(stderr, "Error al ejecutar el comando: %s\n", strerror(errno));
							exit(1);
							
						//Utimo hijo
						}else if(i == (line->ncommands - 1)){
							close(pipes[i-1][1]);
							dup2(pipes[i-1][0],fileno(stdin)); //Solo entrada
							execvp(line->commands[i].argv[0], line->commands[i].argv);
							
							fprintf(stderr, "Error al ejecutar el comando: %s\n", strerror(errno));
							exit(1);
							
						//Resto
						} else {
							close(pipes[i][0]);
							close(pipes[i-1][1]);
							dup2(pipes[i-1][0],fileno(stdin)); //Entrada
							dup2(pipes[i][1],fileno(stdout)); //Salida
							
							execvp(line->commands[i].argv[0], line->commands[i].argv);
							fprintf(stderr, "Error al ejecutar el comando: %s\n", strerror(errno));
							exit(1);
						}
						
					//Padre
					} else {
						if(i != (line->ncommands - 1)){
							close(pipes[i][1]); //Evitar acceso a memoria que no te corresponde
						}
					}

				} else {
					fprintf(stderr, "%s: No se encuentra el mandato\n" , line->commands[i].argv[0]); //Error el mandato no se encuentra
				}
			}

			//Esperamos a todos los hijos y liberamos memoria dinamica
			while((wpid = wait(&status)) > 0);

			for(i = 0; i < line->ncommands-1; i++){
				free(pipes[i]);
			}
			free(pipes);
			fflush(stdin);
			fflush(stdout);
			fflush(stderr);
		}
		
		//Devolver entrada y salida a estandar
		if(line->redirect_input != NULL){
			dup2(Entrada, fileno(stdin));
		}
		if(line->redirect_output != NULL){
			dup2(Salida, fileno(stdout));
		}
		if(line->redirect_error != NULL){
			dup2(Error, fileno(stderr));
		}
		
		//Volvemos a ignorar las señales SGINT y SIGQUIT
		signal(SIGINT, SIG_IGN);
		signal(SIGQUIT, SIG_IGN);
		
		printf("\033[1;31mmsh\033[0m>\033[1;34m~%s\033[0m# ",WhereAmI);
	}

	return 0;
}


void redirect_cd(int num, char * direccion){
	
	if(num > 2){
		printf("Demasiados argumentos\n");
	} else if(num == 1){
		chdir(getenv("HOME"));
		char cwd[512];
		if (getcwd(cwd, sizeof(cwd)) != NULL){
			memset(WhereAmI,'\0',strlen(WhereAmI));
			strcpy(WhereAmI,cwd);
		} else {
			printf("getcwd() error");
			exit(0);
		}
		
	} else if (num==2){
		
		char *gdir;
		char*dir;
		char*to;
		char cwd[512];
		
		gdir=getcwd(cwd, sizeof(cwd));
		dir=strcat(gdir,"/");
		to=strcat(dir, direccion);
		
		if(chdir(to) == 0){ //Como chdir devuelve 0 en caso de exito y -1 en caso de error, de esta manera sabemos si el directorio existe o no
			memset(WhereAmI,'\0',strlen(WhereAmI)); //Vaciamos cadena
			gdir=getcwd(cwd, sizeof(cwd)); //Una vez vacia realizamos esto de nuevo para que cuando se ponga "cd .." no aparezca en nuestra ubicacion msh>X/Y/Z/..
			strcpy(WhereAmI,cwd);
		} else {
			if(chdir(direccion)==0){ //Esto es necesario, ya que en la anterior forma, vamos encapsulando una direccion, asumiendo que es relativa. Aquí se comprueba si es absoluta
				memset(WhereAmI,'\0',strlen(WhereAmI)); //Vaciamos cadena
				gdir=getcwd(cwd, sizeof(cwd)); //Una vez vacia realizamos esto de nuevo para que cuando se ponga "cd .." no aparezca en nuestra ubicacion msh>X/Y/Z/..
				strcpy(WhereAmI,cwd);
			} else {
			printf("%s: No existe la ruta indicada\n", direccion);
			}
		}
	}
}
