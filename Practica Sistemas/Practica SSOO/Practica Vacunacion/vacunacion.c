#include <stdio.h>
#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include <math.h> 
#include <string.h>

//Izan Cruz Ruiz y Santiago Andrés Linares Oliveira

//CONSTANTES

const int BUFFER_SIZE = 1024; // Tamaño de los buffers
const int T_MIN_REPARTO = 1;
const int T_MIN_CITA = 1;
const int T_MIN_DESPLAZAMIENTO = 1;
const int NUM_TANDAS = 10;
const int NUM_FABRICAS = 3;
const int NUM_CENTROS = 5;

//VARIABLES GLOBALES

//Para que no haya problemas con las variables compartidas creamos el mutex 
pthread_mutex_t mutex; 

//Para saber si un centro está disponible para vacunar 
pthread_cond_t *centroVDisponible;

//Fichero Salida 
FILE *fs;

//VARIABLES
int nHabitantes; 			//Numero de habitantes
int nVacunadosTanda;		//Numero de vacunados por tanda
int nVacunasIniciales; 		//Numero de vacunas iniciales por centro (aleatorio).
int nVacunasTotales;		//Numero de vacunas que cada fábrica tiene que fabricar.
int nVacunadosTanda; 		//Numero de habitantes vacunados por tanda.
int nMinVacunasTanda;		//Número mínimo de vacunas producidas por una fábrica en una tanda.
int nMaxVacunasTanda;		//Número máximo de vacunas producidas por una fábrica en una tanda.
int tMinFabricacionTanda;	//Tiempo mínimo de fabricación de una tanda en una fábrica.
int tMaxFabricacionTanda;	//Tiempo máximo de fabricación de una tanda en una fábrica.
int tMaxRepartoTanda; 		//Tiempo Máximo de Reparto de una tanda de vacunas a un centro.
int tMaxDarseCuentaV; 		//Tiempo Máximo de un habitante para darse cuenta de que tiene cita.
int tMaxDesplazamiento; 	//Tiempo Máximo de desplazamiento de un habitante a un centro.

//Para almacenar las vacunas crearemos los siguientes arrays 
int* habitantes; 		//Array donde se guardará futuramente los hilos. Un hilo por habitante.
int* vacunasCentros; 	//Array donde se almacenará el numero de vacunas por centro.
int* vacunasFabricas;	//Array donde se almacenará el numero de vacunas por fábrica.
int* fabricas;  		//Array donde se guardará futuramente los hilos de fábricas. Un hilo por fábrica.

//Arrays para llevar la estadística

int* nVacunasRepartidasxCentro;	//Numero de vacunas repartidas en total (con las 3 fabricas) repartidas por centro.
int** vacunasFabricasCentros;	//Numero de vacunas que cada fabrica ha repartido a cada centro.

//Funciones Varias
void extraerYGuardarDatos(FILE *fd);
void imprimirDatos();
void *fabricar( void* args);
void *vacunar(void* args);
int generadorNumAleatorioTiempoRango(int min, int max);
int generadorNumAleatorioTiempo(int n);
int menosVacunas(int * aux);
void imprimirEstadisticas();
 
int main(int argc, char **argv){
	
	FILE *fe;
	
	int i, j; 		//Contador para los for's
	
	nVacunadosTanda = nHabitantes/NUM_TANDAS;
	nVacunasTotales = nHabitantes/NUM_FABRICAS;
	
	pthread_t *th_habitantes;
	pthread_t *th_fabricas;
	 
	if ((argc == 0) || (argc>3)){
		printf("Error en el numero de argumentos \n"); 
		exit(1); 
	}
	else{
		if (argc == 1){
			fe = fopen("entrada_vacunacion.txt", "r");
			fs = fopen("salida_vacunacion.txt", "w");
		}
		else if(argc == 2){
			if (access(argv[1], F_OK) != -1) {
				fe = fopen(argv[1], "r");
				fs = fopen("salida_vacunacion.txt", "w");
			} else {
				printf("El fichero de entrada no existe.\n");	
				exit(1);
			}		
		}
		else if(argc == 3){
			if ((access(argv[1], F_OK) != -1) || (access(argv[2], F_OK) != -1)) {
				fe = fopen(argv[1],"r");
				fs = fopen(argv[2],"w");
			} else {
				printf("Alguno de los ficheros introducidos no existen.\n");
				exit(1);
			}
		}
	}		
	extraerYGuardarDatos(fe);
	imprimirDatos();
	
	srand(time(NULL));
	
	// Arrays unidireccionales
	th_habitantes = (pthread_t*) malloc (nHabitantes * sizeof(pthread_t));	
	th_fabricas = (pthread_t*) malloc (NUM_FABRICAS* sizeof(pthread_t));
	
	vacunasCentros = (int*) malloc (NUM_CENTROS * sizeof(int));
	vacunasFabricas = (int*) malloc (NUM_FABRICAS * sizeof(int));
	
	habitantes = (int*) malloc (nHabitantes * sizeof(int));
	fabricas = (int*) malloc (NUM_FABRICAS * sizeof(int));
	
	centroVDisponible = (pthread_cond_t*) malloc (NUM_CENTROS* sizeof(pthread_cond_t));
	
	nVacunasRepartidasxCentro = (int*) malloc (NUM_CENTROS* sizeof(int));

	// Array bidimensional

	vacunasFabricasCentros = (int **) malloc (NUM_FABRICAS* sizeof(int*)); 
	
	for (i = 0; i < NUM_CENTROS; i++) {
		vacunasFabricasCentros[i] = (int*) malloc (NUM_CENTROS* sizeof(int)); 
	}

	// Inicializamos el mutex y las condiciones

	pthread_mutex_init(&mutex, NULL);

	for (i = 0; i < NUM_CENTROS; i++) {
		pthread_cond_init(&centroVDisponible[i], NULL);
	}

	// Inicializamos 

	// Variables de los centros

	for (i = 0; i < NUM_CENTROS; i++) {
		vacunasCentros[i] = nVacunasIniciales;
		nVacunasRepartidasxCentro[i] = 0;
	}

	// Variables de las fábricas

	for (i = 0; i < NUM_FABRICAS; i++) {
		vacunasFabricas[i] = nVacunasTotales;
	}

	for (i = 0; i < NUM_FABRICAS; i++) {
		for (j = 0; j < NUM_CENTROS; j++) {
			vacunasFabricasCentros[i][j] = 0;
		}
	}

	// Hilos
	
	// Fabricas
	for (i = 0; i < NUM_FABRICAS; i++) {
		fabricas[i] = i;
		pthread_create(&th_fabricas[i], NULL, fabricar, (void *) &fabricas[i]);
	}
	
	// Habitantes

	for (i = 0; i < NUM_TANDAS; i++) { // Tandas
		for (j = 0; j < nVacunadosTanda; j++) { // Habitantes por tanda
			habitantes[j] = j + (nVacunadosTanda*i);
			pthread_create(&th_habitantes[j], NULL, vacunar, (void *) &habitantes[j]);
		}
		for (j = 0; j < nVacunadosTanda; j++) {
			pthread_join(th_habitantes[j],NULL);
		}	
	}

	for (i = 0; i < NUM_FABRICAS; i++) {
		pthread_join(th_fabricas[i], NULL);
	}

	printf("Vacunacion finalizada.\n\n");
	fprintf(fs, "Vacunacion finalizada.\n\n");

	// Imprimimos por pantalla las estadisticas de la vacunación

	imprimirEstadisticas();

	// Liberamos memoria
	
	free(th_habitantes);
	free(th_fabricas);
	free(vacunasCentros);
	free(vacunasFabricas);
	free(habitantes);
	free(fabricas);
	free(centroVDisponible);
	free(vacunasFabricasCentros);

	// Destruimos el mutex

	pthread_mutex_destroy(&mutex);
	
	// Destruimos las condiciones
	
	for (i = 0; i < NUM_CENTROS; i++) {
		pthread_cond_destroy(&centroVDisponible[i]);
	}

	// Cerramos el fichero de salida

	fclose(fs);
	
	exit(0);
}

void extraerYGuardarDatos(FILE *fd){
	char buffer[BUFFER_SIZE];

	fgets(buffer,6,fd);
	nHabitantes = atoi(buffer); //atoi(buffer) convierte la cadena de caracteres dentro de buffer a su valor entero

	fgets(buffer,6,fd);
	nVacunasIniciales = atoi(buffer);

	fgets(buffer,6,fd);
	nMinVacunasTanda = atoi(buffer);

	fgets(buffer,6,fd);
	nMaxVacunasTanda = atoi(buffer);

	fgets(buffer,6,fd);
	tMinFabricacionTanda = atoi(buffer);

	fgets(buffer,6,fd);
	tMaxFabricacionTanda = atoi(buffer);

	fgets(buffer,6,fd);
	tMaxRepartoTanda = atoi(buffer);

	fgets(buffer,6,fd);
	tMaxDarseCuentaV = atoi(buffer);

	fgets(buffer,6,fd);
	tMaxDesplazamiento = atoi(buffer);
}

void *fabricar(void *args){
	//Hace una encapsulación del puntero a void que le entra, transformandolo en puntero a entero
	//guardando su valor en nFabrica
	int nFabrica = *((int *) args); 
	
	//Si la iésima fabrica aun tiene vacunas por fabricar
	while (vacunasFabricas[nFabrica] > 0) {
		int vacunasxTanda = rand() % (nMaxVacunasTanda - nMinVacunasTanda + 1) + nMinVacunasTanda;
		//variable que guarda la cantidad de vacunas de exceso(en caso de que se llegue al num maximo de vacunas por fabrica: 400)
		int residuo = 0;
		int i, centroMenosVacunas;
		
		int * vacunas; //Numero de vacunas por centro repartidas en esta tanda;
		
		vacunas = (int*) malloc (NUM_CENTROS * sizeof(int));
				
		sleep(generadorNumAleatorioTiempoRango(tMinFabricacionTanda, tMaxFabricacionTanda));		
		
		vacunasFabricas[nFabrica] -= vacunasxTanda;
		if (vacunasFabricas[nFabrica] < 0) { //Si se fabrican más vacunas de las que se necesitan:
			vacunasxTanda += vacunasFabricas[nFabrica];
			//Idicamos que se ha alcanzado el numero máximo de vacunas fabricadas (en esta fabrica).
			vacunasFabricas[nFabrica]=0;
		}
		
		residuo = vacunasxTanda % NUM_CENTROS;
		//Solo se genera residuo en caso de no se pueda repartir el excedente de forma igualitaria entre los 5 centros.
		
		pthread_mutex_lock(&mutex); //Entrando a la región crítica.
		if (vacunasxTanda > 0){
			printf("La Fábrica %i prepara %i vacunas.\n", nFabrica+1, vacunasxTanda);
			fprintf(fs, "La fábrica %i prepara %i vacunas.\n", nFabrica+1, vacunasxTanda);
						
			sleep(generadorNumAleatorioTiempo(tMaxRepartoTanda));
			
			for (i = 0; i < NUM_CENTROS; i++){
				//Actualizamos
				vacunasCentros[i] += vacunasxTanda/NUM_CENTROS;
				vacunasFabricasCentros[nFabrica][i] += vacunasxTanda/NUM_CENTROS;
				nVacunasRepartidasxCentro[i] += vacunasxTanda/NUM_CENTROS;								
				vacunas[i] += vacunasxTanda/NUM_CENTROS;
			}
			
			//Terminamos de repartir las vacunas residuales (en caso de que hayan).
			while (residuo > 0) {
				centroMenosVacunas = menosVacunas(vacunasCentros);
				
				vacunasCentros[centroMenosVacunas]++;
				vacunasFabricasCentros[nFabrica][centroMenosVacunas]++;
				nVacunasRepartidasxCentro[centroMenosVacunas]++;
				vacunas[centroMenosVacunas]++;
				
				residuo--;
			}
			
			for (i = 0; i < NUM_CENTROS; i++){
				printf("Fabrica %i entrega %i vacunas en el centro %i.\n", nFabrica+1,vacunas[i],i+1);
				fprintf(fs,"Fábrica %i entrega %i vacunas en el centro %i.\n", nFabrica+1, vacunas[i], i+1);
			}
		}
			
			//En caso de que alguna fabrica haya fabricado el maximo de vacunas.
			if (vacunasFabricas[nFabrica]== 0) {
				printf("Fabrica %i ha fabricado todas sus vacunas. \n", nFabrica+1);
				fprintf(fs,"Fabrica %i ha fabricado todas sus vacunas. \n", nFabrica+1);
				
				}
			
			pthread_mutex_unlock(&mutex);
			
			//Vemos si todos los centros están disponibles para vacunar
			for (int i=0; i< NUM_CENTROS; i++){
				pthread_cond_signal(&centroVDisponible[i]);
			}		
		free(vacunas);				
		
	}
	pthread_exit(NULL);
}

void *vacunar(void *args){
	//Hace una encapsulación del puntero a void que le entra, transformandolo en puntero a entero
	//guardando su valor en nHabitantes
	int nHabitantes = *((int *) args);
	//Genereamos un numero aleatorio, para seleccionar el centro elegido
	int i = (rand() % NUM_CENTROS);
	
	//Tiempo que tarda en darse cuenta de que ha sido citado para la vacuna 	
	sleep(rand() % tMaxDarseCuentaV + 1);
	
	printf("Habitante %i elige el centro %i para vacunarse. \n", nHabitantes+1, i+1);
	fprintf(fs, "Habitante %i elige el centro %i para vacunarse. \n", nHabitantes+1, i+1);
	
	//Tiempo que tarda en desplazarse al centro elegido
	sleep(rand() % tMaxDesplazamiento + 1);
	
	pthread_mutex_lock(&mutex); //Entramos en la zona critica
	
	
	//Si no quedan vacunas en los centros, esperamos hasta recibir la señal
	while (vacunasCentros[i] == 0){
		pthread_cond_wait(&centroVDisponible[i],&mutex);
	}
	
	//Vemos si hay vacunas disponibles en un centro y si las hay las aplicamos 
	if (vacunasCentros[i] > 0){
		vacunasCentros[i]--;
	}
	
	//Activamos la señal 
	pthread_cond_signal(&centroVDisponible[i]);
	
	pthread_mutex_unlock(&mutex); // Salimos de la zona critica
	
	printf("Habitante %i vacunado en el centro %i.\n", nHabitantes+1, i+1);
	fprintf(fs,"Habitante %i vacunado en el centro %i.\n", nHabitantes+1, i+1);
	
	pthread_exit(NULL);	
}

int menosVacunas(int * aux) {
	int i, menor, centroMenosVacunas;
	menor = aux[0];
	centroMenosVacunas = 0;
	for (i = 0; i < NUM_CENTROS; i++) {
		if (aux[i] < menor) {
				menor = aux[i];
				centroMenosVacunas = i;
		}
	}
	return centroMenosVacunas;
}

int generadorNumAleatorioTiempoRango(int min, int max){
    //Inicializamos el numero aleatorio
    srand(time(NULL));
    //Generar y devolver un numero aleatorio entre min y max
    return rand() % (max - min + 1) + min;
}

int generadorNumAleatorioTiempo(int n){
	srand(time(NULL));
	return (rand() % n +1);
}

void imprimirDatos () {

	nVacunadosTanda = nHabitantes/NUM_TANDAS;
	nVacunasTotales = nHabitantes/NUM_FABRICAS;

	printf("vacunar EN PANDEMIA: CONFIGURACION INICIAL \n");
    	fprintf(fs,"vacunar EN PANDEMIA: CONFIGURACION INICIAL \n");
        printf("Habitantes: %i \n", nHabitantes);
        fprintf(fs,"Habitantes: %i \n", nHabitantes);
        printf("Centros de vacunar: %i \n", NUM_CENTROS);
        fprintf(fs,"Centros de vacunar: %i \n", NUM_CENTROS);
        printf("Fabricas: %i \n", NUM_FABRICAS);
        fprintf(fs,"Fabricas: %i \n", NUM_FABRICAS);
        printf("Vacunados por tandas: %i \n", nVacunadosTanda);
        fprintf(fs,"Vacunados por tandas: %i \n", nVacunadosTanda);
        printf("Vacunas Iniciales en cada centro: %i \n", nVacunasIniciales);
        fprintf(fs,"Vacunas Iniciales en cada centro: %i \n", nVacunasIniciales);
        printf("Vacunas Totales por fabrica: %i \n", nVacunasTotales);
        fprintf(fs,"Vacunas Totales por fabrica: %i \n", nVacunasTotales);
		printf("Mínimo número de vacunas fabricadas en cada tanda: %i \n",nMinVacunasTanda);
        fprintf(fs, "Mínimo número de vacunas fabricadas en cada tanda: %i \n",nMinVacunasTanda);
        printf("Máximo número de vacunas fabricadas en cada tanda: %i \n",nMaxVacunasTanda);
        fprintf(fs,"Máximo número de vacunas fabricadas en cada tanda: %i \n",nMaxVacunasTanda);
        printf("Tiempo mínimo de fabricación de una tanda de vacunas: %i \n", tMinFabricacionTanda);
        fprintf(fs,"Tiempo mínimo de fabricación de una tanda de vacunas: %i \n", tMinFabricacionTanda);
        printf("Tiempo máximo de fabricación de una tanda de vacunas: %i \n", tMaxFabricacionTanda);
        fprintf(fs,"Tiempo máximo de fabricación de una tanda de vacunas: %i \n", tMaxFabricacionTanda);
        printf("Tiempo máximo de reparto de vacunas a los centros: %i \n", tMaxRepartoTanda);
        fprintf(fs,"Tiempo máximo de reparto de vacunas a los centros: %i \n", tMaxRepartoTanda);
        printf("Tiempo máximo que un habitante tarda en ver que está citado para vacunarse: %i \n", tMaxDarseCuentaV);
        fprintf(fs,"Tiempo máximo que un habitante tarda en ver que está citado para vacunarse: %i \n", tMaxDarseCuentaV);
        printf("Tiempo máximo de desplazamiento del habitante al centro de vacunación: %i \n",tMaxDesplazamiento);
        fprintf(fs,"Tiempo máximo de desplazamiento del habitante al centro de vacunación: %i \n",tMaxDesplazamiento);
        printf("\n");
        fprintf(fs,"\n");
        printf("PROCESO DE vacunar\n");
        fprintf(fs,"PROCESO DE vacunar\n");
}

void imprimirEstadisticas(){
	int i,j;
	
	nVacunasTotales = nHabitantes/NUM_FABRICAS;
	
	printf("Estadísticas\n");
	fprintf(fs, "Estadísticas\n");
	
	for (i = 0; i < NUM_FABRICAS; i++){
		printf("\nFabrica %i:\n", i+1);
		fprintf(fs, "\nFabrica %i:\n", nVacunasTotales);
		printf("Ha fabricado %i vacunas.\n", nVacunasTotales);
		fprintf(fs, "Ha fabricado %i vacunas.\n", nVacunasTotales);
		
		for (j = 0; j < NUM_CENTROS; j++){
			printf("vacunas entregadas al centro %i: %i.\n", j+1, vacunasFabricasCentros[i][j]);			
			fprintf(fs, "vacunas entregadas al centro %i: %i.\n", j+1, vacunasFabricasCentros[i][j]);			
		}
	}
	
	for (i = 0; i< NUM_CENTROS; i++){
		printf("\nCentro %i:\n", i+1);
		fprintf(fs, "\nCentro %i:\n", i+1);
		printf("Ha recibido %i vacunas.\n", nVacunasRepartidasxCentro[i]);
		fprintf(fs, "Ha recibido %i vacunas.\n", nVacunasRepartidasxCentro[i]);
		printf("Se han vacunado %i habitantes.\n", (nVacunasRepartidasxCentro[i] + nVacunasIniciales) - vacunasCentros[i]);
		fprintf(fs, "Se han vacunado %i habitantes.\n", (nVacunasRepartidasxCentro[i] + nVacunasIniciales) - vacunasCentros[i]);
		printf("Han sobrado %i vacunas.\n", vacunasCentros[i]);
		fprintf(fs, "Han sobrado %i vacunas.\n", vacunasCentros[i]);
	}
}
