#include <stdlib.h>
#include <stdio.h>
#include "parser.h"
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#define MAX_PROCESOS 5

void ejecutar_comandos(tline * line);
void redirecciones(tline * line);
// void annadirlista();

char buffer[1024];


 //Definimos el tipo estructurado si hiciesemos el jobs y el fg, dado que habría que hacer
 // insercciones en la lista, en caso de que se ejecutase en segundo plano. Para luego al 
 // hacer el fg sacarlas de esa lista y esperar a que se ejecute en primer plano, mediante 
 // el wait(status)
typedef struct {
	pid_t pid; 
	int n;
	char cmds[1024];
} background_process;

background_process list[MAX_PROCESOS]; 
int cont_list = 0;


void ignorar_sigint() {
	fflush(stdin);
	printf("\n");
	signal(SIGINT, ignorar_sigint);
}


 //Programa Principal donde ejecutamos todos los comandos 
int main(int argc, char * argv[]) {

  //Tipo personalizado en el que se guarda lo que se escribe en la linea de comandos. 
  tline * line;

  //Es una secuencia de escape ANSI para cambiar el color de fondo de la cadena "msh>$".
  printf("\x1b[32mmsh>$ \x1b[0m");
  
  //Ignoramos la señal SIGINT -> CTRL + C
  signal(SIGINT, ignorar_sigint);
  signal(SIGQUIT, SIG_DFL);
	
 //Bucle de lectura de la línea	
  while (fgets(buffer, 1024, stdin)) {

  //Divide la informacion en fragmentos y lo guarda en el tline line
    line = tokenize(buffer);

    if (line != NULL) {
      ejecutar_comandos(line);
    }

    if (line -> background) {
      printf("El comando se ejecutara en segundo plano \n");
     
     /* 
      int pidb; 
      pidb = fork();
      
      if(pidb == 0){ //Proceso Hijo
		  signal(SIGINT, ignorar_sigint);
		  redirecciones(line);
		  ejecutar_comandos(line); 
	  }
	  else 
		annadirlista(list);*/ 
    }

    //Cambia colores de "msh>$"
    printf("\x1b[32mmsh>$ \x1b[0m");
   

    signal(SIGINT, ignorar_sigint);
    signal(SIGQUIT, SIG_IGN);

  }
  return 0;
}

/*
void annadirlista(background_process list){
	 //Añadir a la lista
	list[cont_list].pid = pid; 
	list[cont_list].cmds = cmds; 
	list[cont_list].n = n; 
	cont_list++; 
	
	
}*/


/*
void comandojobs(){
	for (int i = 0; i<cont_list; i++){
		printf("[%d] Running &s", list.n, list.cmds);
	}

}*/


//Comando cd, que se basa principalmente en el valor devuelto por open, que se asigna a la 
// variable dir
void comandocd(tline * line) {
  char * home;
  int dr;
  if (line -> commands[0].argv[1] != NULL) {
    //chdir devuelve 0 si el cambio de directorio es exitoso
    //Sino devuelve -1
    dr  = chdir(line -> commands[0].argv[1]);
    if (dr < 0) {
      fprintf(stderr, "%s: Directorio Incorrecto\n ", line -> commands[0].argv[1]);
    } else {
      printf("Cambiando a directorio: %s\n", line -> commands[0].argv[1]);
    }
  } else { //Si no se pasa ninguna direccion para cambiarse, se redirige a home
    //getenv() devuelve un puntero a una cadena de caracteres que vontiene el valor de la variable de entorno
    home = getenv("HOME");
    dr = chdir(home);
    if (dr < 0) {
      fprintf(stderr, "%s: Directorio Incorrecto\n", home);
    } else {
      printf("Cambiando a directorio: %s\n", home);
    }
  }

}

void comandoumask(tline * line) {
  mode_t mask;

  if (line -> commands[0].argv[1] == NULL) {
    // Recupera el valor de la máscara de permisos
    mask = umask(0);
    // Imprime el valor de la máscara de permisos en formato numérico
    printf("Máscara de permisos numérica: %04o\n", mask);
    umask(mask);
  } else {
    // Convierte el valor de la máscara de permisos de octal a decimal
    mask = strtol(line -> commands[0].argv[1], NULL, 8);
    // Establece el valor de la máscara de permisos
    umask(mask);
  }
}

void ejecutar_comandos(tline * line) {
  int status, i, j, k;
  pid_t pid;
  int ** p;
	 //Dividimos en dos grandes partes, si solo recibe un comando 
  if (line -> ncommands == 1) {

    if (strcmp(line -> commands[0].argv[0], "cd") == 0) {
      comandocd(line);
      return;
    }
    if (strcmp(line -> commands[0].argv[0], "exit") == 0) {
      exit(0);
    }

    if (strcmp(line -> commands[0].argv[0], "umask") == 0) {
      comandoumask(line);
      return;
    }
    /*
    if (strcmp(line -> commands[0].argv[0], "jobs") == 0) {
      comandojobs();
      return;
    }*/

    pid = fork();

    if (pid < 0) {
      fprintf(stderr, "Fallo el fork");
    } else if (pid == 0) { //PROCESO HIJO

      signal(SIGINT, SIG_DFL);
      signal(SIGQUIT, SIG_DFL);

      redirecciones(line);

      if (line -> commands[0].filename != NULL) {
				execvp(line -> commands[0].argv[0], line -> commands[0].argv);
        //Si el execvp es exitoso, no continua este camino de ejecucion, sino el camino del comando que se le pida.
        //por lo que si es exitoso, saldra con exit(0) en el programa especificado a ejecutar, p.e. ls.

        //Si no es exitoso, devuelve -1 y sigue este camino de ejecución imprimiendo por pantalla y haciendo exit(1)
        fprintf(stderr, "Se ha producido un error.\n");
        exit(1);
      } else {
        fprintf(stderr, "No se encuentra el mandato %s\n", line -> commands[0].argv[0]);
        exit(1);
      }
    } else { //PROCESO PADRE
      wait( & status);
      if (WIFEXITED(status) && WEXITSTATUS(status) != 0) {
        printf("El comando no se ejecuto correctamente\n");
      }
    }
    
    //Si recibe más de un comando 
  } else if (line -> ncommands > 1) {
    //Creamos el array bidimensional para almacenar los pipes
    //Ponemos ncomands -1 porque para 2 mandatos,solo hace falta un pipe.
    //Para 3, serían 2 pipes, y así sucesivamente.
    p = (int ** ) malloc(sizeof(int * ) * line -> ncommands - 1);
    for (i = 0; i < line -> ncommands; i++) {
      p[i] = malloc(sizeof(int) * 2); //Creamos la parte bidemensional de los pipes
      pipe(p[i]);
    }

    for (i = 0; i < line -> ncommands; i++) {
      //Seguimos el mismo proceso seguido en todos los procesos
      pid = fork();

      if (pid < 0) {
        fprintf(stderr, "Ha fallado el fork\n");
        return;
      } else if (pid == 0) { //PROCESO HIJO
        if ((line -> commands[0].filename) != NULL) {//Si el mandato existe
          signal(SIGINT, SIG_DFL);
          signal(SIGQUIT, SIG_DFL);

          if (i == 0) { //Primer mandato
            redirecciones(line);
            //Si al hacer la redirección de la salida estandar (1), da error
            if (dup2(p[0][1], 1) == -1) {
              fprintf(stderr, "Fallo al hacer dup\n");
              exit(1);
            }
          } else if (i == line -> ncommands - 1) { //Ultimo mandato	
            redirecciones(line);
            //Si al hacer la redireccion de la entrada estandar (0) da error.
            if (dup2(p[i - 1][0], 0) == -1) {
              fprintf(stderr, "Fallo al hacer dup\n");
              exit(1);
            }
          } else { //Resto de mandatos
            if ((dup2(p[i - 1][0], 0) == -1) || (dup2(p[i][1], 1) == -1)) {
              fprintf(stderr, "Fallo al hacer dup\n");
              exit(1);
            }
          }
        } else {
          fprintf(stderr, "No se encuentra el mandato %s\n", line -> commands[0].argv[0]);
          exit(1);
        }

        for (j = 0; j < line -> ncommands - 1; j++) { //Cerrar los pipes
          for (k = 0; k < 2; k++) {
            close(p[j][k]);
          }
        }
        //Diferencia entre execv y execvp: 
        //execv(ruta completa del mandato, array con los argumentos del mandato (argv))
        //execvp(nombre mandato, array con los argumentos del comando (argv))
        execv(line -> commands[i].filename, line -> commands[i].argv);
        //Si el execv no se ejecuta con exito, se sigue con al ejecucion y se imprime el error.
        fprintf(stderr, "Se ha producido un error\n");
        exit(1);
      }
    }
  }

  for (i = 0; i < line -> ncommands - 1; i++) { //Cerramos todos los pipes
    for (j = 0; j < 2; j++) {
      close(p[i][j]);
    }
  }

  for (i = 0; i < line -> ncommands; i++) { //Esperamos recibir el estado del hijo
    wait( & status);
  }
  for (i = 0; i < line -> ncommands - 1; i++) { //Liberamos la memoria
    free(p[i]);
  }
}

void redirecciones(tline * line) {
  int rd;
  //Existen tres tipos de entrada que son las opciones que vamos a contemplar

  if (line -> redirect_input != NULL) {
    rd = open(line -> redirect_input, O_CREAT | O_RDONLY, 0666);
    //Abrimos o creamos el fichero, con los permisos estandar
    if (rd == -1) {
      fprintf(stderr, "%s: ERROR", line -> redirect_input);
      exit(1);
    } else {
      dup2(rd, fileno(stdin));
    }
  } else if (line -> redirect_output != NULL) {
    rd = open(line -> redirect_output, O_CREAT | O_RDWR, 0666);
    //Abrimos o creamos el fichero, con los permisos estandar
    if (rd == -1) {
      fprintf(stderr, "%s: ERROR", line -> redirect_output);
      exit(1);
    } else {
      //rd es un descriptor de fichero que hemos abierto anteriormente
      //fileno obtiene el descriptor de fichero asociado al flujo de salida.
      //con el dup2 se redirige la salida del descriptor de fichero stdout al descriptor de fichero fd
      dup2(rd, fileno(stdout));
    }
  } else if (line -> redirect_error != NULL) {
    rd = open(line -> redirect_error, O_CREAT | O_RDWR, 0666);
    //Abrimos o creamos el fichero, con los permisos estandar
    if (rd == -1) {
      fprintf(stderr, "%s: ERROR", line -> redirect_error);
      exit(1);
    } else {
      dup2(rd, fileno(stderr));
    }
  }
}
